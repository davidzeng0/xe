#include "ws.h"
#include "http_internal.h"
#include "../random.h"
#include "xutil/inet.h"
#include "../conn.h"
#include "../request.h"
#include "xutil/encoding.h"
#include "xutil/writer.h"
#include "xutil/container/localarray.h"
#include <wolfssl/wolfcrypt/sha.h>

using namespace xurl;

class xe_websocket_data_internal;
class xe_websocket : public xe_http_protocol{
public:
	xe_rng random;

	xe_websocket(xurl_ctx& ctx);

	int start(xe_request_internal& request);

	int transferctl(xe_request_internal& request, uint flags);
	void end(xe_request_internal& request);

	int open(xe_request_internal& request, xe_url&& url);

	bool matches(const xe_string_view& scheme) const;

	void redirect(xe_request_internal& request, xe_string&& url);
	int internal_redirect(xe_request_internal& request, xe_string&& url);
	int open(xe_websocket_data_internal& data, xe_url&& url, bool redirect);

	~xe_websocket();

	static xe_cstr class_name(){
		return "xe_websocket";
	}
};

class xe_websocket_connection;
class xe_websocket_data_internal : public xe_websocket_data, public xe_http_internal_data{
protected:
	friend class xe_websocket;
public:
	struct xe_callbacks{
		xe_websocket_ready_cb ready;
		xe_websocket_ping_cb ping;
		xe_websocket_ping_cb pong;
		xe_websocket_message_cb message;
	} callbacks;

	xe_localarray<byte, 24> key;
	xe_localarray<byte, 28> accept;
};

enum xe_websocket_opcode{
	WS_CONTINUATION = 0x0,
	WS_TEXT = 0x1,
	WS_BINARY = 0x2,
	WS_CLOSE = 0x8,
	WS_PING = 0x9,
	WS_PONG = 0xA
};

enum{
	WS_NONE = 0,
	WS_FRAME_HEADER_FIRST,
	WS_FRAME_HEADER_SECOND,
	WS_FRAME_HEADER_LENGTH1,
	WS_FRAME_HEADER_LENGTH2,
	WS_FRAME_HEADER_LENGTH3,
	WS_FRAME_HEADER_LENGTH4,
	WS_FRAME_HEADER_LENGTH5,
	WS_FRAME_HEADER_LENGTH6,
	WS_FRAME_HEADER_LENGTH7,
	WS_FRAME_HEADER_LENGTH8,
	WS_FRAME_DATA
};

enum{
	MAX_MESSAGE_SIZE = 1024 * 1024
};

class xe_websocket_connection : public xe_http_singleconnection{
protected:
	bool connection_upgrade_seen: 1;
	bool upgrade_websocket_seen: 1;
	bool websocket_accept_seen: 1;
	bool failure: 1;
	bool is_first_fragment: 1;

	xe_websocket_data_internal& options(){
		return *(xe_websocket_data_internal*)specific;
	}
public:
	struct send_list{
		send_list* next;
		size_t len;
		byte data[];
	};

	byte fin, rsv, opcode;
	byte mask;
	uint ws_state;
	ulong payload_len;
	size_t max_size = MAX_MESSAGE_SIZE;

	xe_vector<byte> message;

	send_list* send_head;
	send_list* send_tail;
	size_t offset;

	xe_websocket_connection(xe_websocket& proto): xe_http_singleconnection(proto){}

	bool readable(){
		return ws_state == WS_NONE ? xe_http_singleconnection::readable() : send_head != null;
	}

	int writable(){
		if(ws_state == WS_NONE)
			return xe_http_singleconnection::readable();
		xe_assert(send_head != null);

		send_list& data = *send_head;
		size_t result;

		result = xe_connection::send(data.data + offset, data.len - offset);

		xe_log_trace(this, "sent %zi bytes", result);

		if(result <= 0){
			if(!result) return XE_SEND_ERROR;
			if(result == XE_EAGAIN) return 0;

			return result;
		}

		offset += result;

		if(offset == data.len){
			send_head = data.next;
			offset = 0;

			if(!send_head)
				send_tail = null;
			xe_dealloc(&data);
		}

		return 0;
	}

	int handle_statusline(xe_http_version version, uint status, xe_string_view& reason){
		if(status != 101)
			failure = true;
		return 0;
	}

	int handle_singleheader(xe_string_view& key, xe_string_view& value){
		if(failure)
			return 0;
		if(key.equal_case("Connection")){
			if(value.equal_case("upgrade"))
				connection_upgrade_seen = true;
			else
				failure = true;
		}else if(key.equal_case("Upgrade")){
			if(value.equal_case("websocket"))
				upgrade_websocket_seen = true;
			else
				failure = true;
		}else if(key.equal_case("Sec-WebSocket-Accept")){
			websocket_accept_seen = true;

			if(xe_string_view((xe_slice<const byte>)options().accept) != value)
				failure = true;
		}

		return 0;
	}

	int pretransfer(){
		if(failure || !connection_upgrade_seen || !upgrade_websocket_seen || !websocket_accept_seen) return XE_WEBSOCKET_CONNECTION_REFUSED;
		if(options().callbacks.ready && options().callbacks.ready(*request)) return XE_ABORTED;

		ws_state = WS_FRAME_HEADER_FIRST;
		bodyless = false;
		is_first_fragment = true;

		return xe_http_singleconnection::pretransfer();
	}

	int write_body(xe_bptr buf, size_t len){
		size_t wlen;

		while(len){
			switch(ws_state){
				case WS_FRAME_HEADER_FIRST:
					fin = buf[0] & 0x80;
					rsv = buf[0] & 0x70;

					if(is_first_fragment){
						opcode = buf[0] & 0xf;
						is_first_fragment = false;
					}

					ws_state = WS_FRAME_HEADER_SECOND;
					buf++;
					len--;

					break;
				case WS_FRAME_HEADER_SECOND:
					mask = buf[0] & 0x80;
					payload_len = buf[0] & 0x7f;
					ws_state = WS_FRAME_DATA;

					if(payload_len == 0x7e){
						ws_state = WS_FRAME_HEADER_LENGTH2;
						payload_len = 0;
					}else if(payload_len == 0x7f){
						ws_state = WS_FRAME_HEADER_LENGTH8;
						payload_len = 0;
					}

					buf++;
					len--;

					if(mask)
						return XE_INVALID_RESPONSE;
					break;
				case WS_FRAME_HEADER_LENGTH1:
				case WS_FRAME_HEADER_LENGTH2:
				case WS_FRAME_HEADER_LENGTH3:
				case WS_FRAME_HEADER_LENGTH4:
				case WS_FRAME_HEADER_LENGTH5:
				case WS_FRAME_HEADER_LENGTH6:
				case WS_FRAME_HEADER_LENGTH7:
				case WS_FRAME_HEADER_LENGTH8:
					payload_len <<= 8;
					payload_len |= buf[0];
					buf++;
					len--;
					ws_state--;

					if(ws_state == WS_FRAME_HEADER_SECOND)
						ws_state = WS_FRAME_DATA;
					break;
				case WS_FRAME_DATA:
					wlen = xe_min(len, payload_len);

					if(payload_len > max_size - message.size() || max_size - message.size() < wlen || !message.append(buf, wlen))
						return XE_ENOMEM;
					len -= wlen;
					buf += wlen;
					payload_len -= wlen;

					if(!payload_len){
						ws_state = WS_FRAME_HEADER_FIRST;

						if(!fin)
							break;
						if(options().callbacks.message && options().callbacks.message(*request, (xe_websocket_message_type)opcode, message))
							return XE_ABORTED;
						message.resize(0);
						is_first_fragment = true;
					}

					break;
			}
		}

		return 0;
	}

	int ping(xe_websocket_opcode type){
		return 0;
	}

	send_list* make_send_node(size_t len){
		send_list* node = (send_list*)xe_zalloc<byte>(sizeof(send_list) + len);

		if(!node)
			return null;
		if(!send_head){
			send_head = node;
			send_tail = node;
		}else{
			send_tail -> next = node;
			send_tail = node;
		}

		node -> len = len;

		return node;
	}

	int send(xe_websocket_message_type type, xe_cptr buf, size_t len){
		xe_localarray<byte, 14> header;
		xe_writer writer(header);
		byte payload_len = len;
		size_t sent = 0, result;

		if(len > 0xffff)
			payload_len = 0x7f;
		else if(len > 0x7d)
			payload_len = 0x7e;
		writer.write<byte>(0x80 | type);
		writer.write<byte>(0x80 | payload_len);

		if(payload_len == 0x7e)
			writer.write(xe_htons(len));
		else if(payload_len == 0x7f)
			writer.write(xe_htonll(len));
		writer.write<uint>(0);

		if(send_head){
		queue:
			send_list* node = make_send_node(len + writer.count() - sent);

			if(!node)
				return XE_ENOMEM;
			if(sent < writer.count()){
				xe_memcpy(node -> data, header.data() + sent, writer.count() - sent);
				xe_memcpy(node -> data + writer.count() - sent, buf, len);
			}else{
				xe_memcpy(node -> data, buf, len + writer.count() - sent);
			}

			transferctl(XE_RESUME_SEND);

			return 0;
		}

		result = xe_connection::send(&header, writer.count());

		if(result <= 0){
			if(!result) return XE_SEND_ERROR;
			if(result != XE_EAGAIN) return result;

			goto queue;
		}

		sent += result;

		if(result < writer.count())
			goto queue;
		result = xe_connection::send(buf, len);

		if(result <= 0){
			if(!result) return XE_SEND_ERROR;
			if(result != XE_EAGAIN) return result;

			goto queue;
		}

		sent += result;

		if(result < len)
			goto queue;
		return 0;
	}

	xe_cstr class_name(){
		return "xe_websocket_connection";
	}
};

xe_websocket::xe_websocket(xurl_ctx& ctx): xe_http_protocol(ctx, XE_PROTOCOL_WEBSOCKET){
	uint seed;

	if(xe_crypto_random(&seed, sizeof(seed)))
		random.seed(seed);
	else
		random.seed();
}

int xe_websocket::start(xe_request_internal& request){
	xe_websocket_data_internal& data = *(xe_websocket_data_internal*)request.data;

	uint port = data.port;
	int err;

	bool secure = data.url.scheme().length() == 3;

	if(!port)
		port = data.url.port();
	if(!port){
		port = secure ? 443 : 80;

		xe_log_debug(this, "using default port %u", port);
	}

	xe_websocket_connection* connection = xe_znew<xe_websocket_connection>(*this);

	if(!connection)
		return XE_ENOMEM;
	connection -> set_ssl_verify(data.ssl_verify);
	connection -> set_connect_timeout(data.connect_timeout);
	connection -> set_ip_mode(data.ip_mode);
	connection -> set_recvbuf_size(data.recvbuf_size);

	do{
		if((err = connection -> init(*ctx)))
			break;
		if(secure && (err = connection -> init_ssl(ctx -> ssl())))
			break;
		if((err = connection -> connect(data.url.hostname(), port)))
			break;
		connection -> open(request);

		return 0;
	}while(false);

	connection -> close(err);

	return err;
}

int xe_websocket::transferctl(xe_request_internal& request, uint flags){
	xe_websocket_data_internal& data = *(xe_websocket_data_internal*)request.data;

	return data.connection -> transferctl(request, flags);
}

void xe_websocket::end(xe_request_internal& request){
	xe_websocket_data_internal& data = *(xe_websocket_data_internal*)request.data;

	data.connection -> end(request);
}

int xe_websocket::open(xe_request_internal& request, xe_url&& url){
	xe_websocket_data_internal* data;

	int err;

	if(request.data && request.data -> id() == XE_PROTOCOL_WEBSOCKET)
		data = (xe_websocket_data_internal*)request.data;
	else{
		data = xe_znew<xe_websocket_data_internal>();

		if(!data) return XE_ENOMEM;
	}

	if((err = open(*data, std::move(url), false))){
		if(data != request.data)
			xe_delete(data);
		return err;
	}

	xe_log_verbose(this, "opened ws request for: %s", data -> url.href().c_str());

	request.data = data;

	return 0;
}

int xe_websocket::open(xe_websocket_data_internal& data, xe_url&& url, bool redirect){
	xe_string_view accept = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	xe_localarray<byte, 16> rng;
	xe_localarray<byte, SHA_DIGEST_SIZE> sum;
	Sha sha;

	xe_return_error(xe_http_protocol::open(data, std::move(url), redirect));

	random.random_bytes(rng.data(), rng.size());

	xe_base64_encode(XE_BASE64_PAD, data.key.data(), data.key.size(), rng.data(), rng.size());

	wc_InitSha(&sha);
	wc_ShaUpdate(&sha, data.key.data(), data.key.size());
	wc_ShaUpdate(&sha, (byte*)accept.c_str(), accept.length());
	wc_ShaFinal(&sha, sum.data());
	xe_base64_encode(XE_BASE64_PAD, data.accept.data(), data.accept.size(), sum.data(), sum.size());

	if(!data.internal_set_header("Connection", "Upgrade", 0) ||
		!data.internal_set_header("Upgrade", "websocket", 0) ||
		!data.internal_set_header("Sec-WebSocket-Version", "13", 0) ||
		!data.internal_set_header("Sec-WebSocket-Key", xe_string_view((xe_slice<const byte>)data.key), 0))
		return XE_ENOMEM;
	return 0;
}

bool xe_websocket::matches(const xe_string_view& scheme) const{
	return scheme == "ws" || scheme == "wss";
}

void xe_websocket::redirect(xe_request_internal& request, xe_string&& url){
	int err = internal_redirect(request, std::move(url));

	if(err) request.complete(err);
}

int xe_websocket::internal_redirect(xe_request_internal& request, xe_string&& url){
	xe_websocket_data_internal& data = *(xe_websocket_data_internal*)request.data;
	xe_url parser(std::move(url));

	if(data.redirects++ >= data.max_redirects)
		return XE_TOO_MANY_REDIRECTS;
	xe_return_error(parser.parse());

	if(!matches(parser.scheme()))
		return XE_EXTERNAL_REDIRECT;
	xe_return_error(open(data, std::move(parser), true));

	return start(request);
}

xe_websocket::~xe_websocket(){}

xe_websocket_data::xe_websocket_data(): xe_http_common_data(XE_PROTOCOL_WEBSOCKET){}

int xe_websocket_data::send(xe_websocket_message_type type, xe_cptr data, size_t size){
	xe_websocket_connection& connection = *(xe_websocket_connection*)(((xe_websocket_data_internal*)this) -> connection);

	return connection.send(type, data, size);
}

int xe_websocket_data::ping(){
	xe_websocket_connection& connection = *(xe_websocket_connection*)(((xe_websocket_data_internal*)this) -> connection);

	return connection.ping(WS_PING);
}

int xe_websocket_data::pong(){
	xe_websocket_connection& connection = *(xe_websocket_connection*)(((xe_websocket_data_internal*)this) -> connection);

	return connection.ping(WS_PONG);
}

void xe_websocket_data::set_ready_cb(xe_websocket_ready_cb cb){
	xe_websocket_data_internal& internal = *(xe_websocket_data_internal*)this;

	internal.callbacks.ready = cb;
}

void xe_websocket_data::set_ping_cb(xe_websocket_ping_cb cb){
	xe_websocket_data_internal& internal = *(xe_websocket_data_internal*)this;

	internal.callbacks.ping = cb;
}

void xe_websocket_data::set_message_cb(xe_websocket_message_cb cb){
	xe_websocket_data_internal& internal = *(xe_websocket_data_internal*)this;

	internal.callbacks.message = cb;
}

xe_websocket_data::~xe_websocket_data(){}

xe_protocol* xurl::xe_websocket_new(xurl_ctx& ctx){
	return xe_new<xe_websocket>(ctx);
}