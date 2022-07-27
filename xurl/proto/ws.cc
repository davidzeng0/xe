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

struct xe_websocket_callbacks{
	xe_websocket_ready_cb ready;
	xe_websocket_ping_cb ping;
	xe_websocket_message_cb message;
};

class xe_websocket_connection;
class xe_websocket_data_internal : public xe_websocket_data, public xe_http_internal_data{
protected:
	friend class xe_websocket;
public:
	xe_websocket_callbacks callbacks;

	xe_localarray<byte, 24> key;
	xe_localarray<byte, 28> accept;
};

enum xe_websocket_opcode{
	WS_CONTINUATION = 0x0,
	WS_TEXT = 0x1,
	WS_BINARY = 0x2,
	WS_CLOSE = 0x8,
	WS_PING = 0x9,
	WS_PONG = 0xa
};

enum{
	WS_NONE = 0,
	WS_FRAME_HEADER_FIRST,
	WS_FRAME_HEADER_SECOND,
	WS_FRAME_HEADER_LENGTH,
	WS_FRAME_DATA
};

enum{
	MAX_MESSAGE_SIZE = 100 * 1024 * 1024
};

struct message_queue{
	message_queue* next;
	size_t len;
	byte data[];
};

class xe_websocket_connection : public xe_http_singleconnection{
protected:
	xe_websocket_data_internal& options(){
		return *(xe_websocket_data_internal*)specific;
	}

	template<typename F, typename... Args>
	bool call(F xe_websocket_callbacks::*field, Args&& ...args){
		auto callback = options().callbacks.*field;
		return callback && callback(std::forward<Args>(args)...) ? true : false;
	}
public:
	bool connection_upgrade_seen: 1;
	bool upgrade_websocket_seen: 1;
	bool websocket_accept_seen: 1;
	bool failure: 1;

	bool is_first_fragment: 1;
	bool is_control: 1;
	bool is_fin: 1;

	uint payload_len_size;
	uint message_state;
	xe_websocket_opcode opcode;
	xe_websocket_opcode current_message_opcode;
	ulong payload_len;
	size_t max_message_size = MAX_MESSAGE_SIZE;

	xe_localarray<byte, 0x7e> control_frame_data;
	xe_writer control_frame_writer;
	xe_vector<byte> current_message_data;

	message_queue* message_head;
	message_queue* message_tail;
	size_t send_offset;

	xe_websocket_connection(xe_websocket& proto): xe_http_singleconnection(proto), control_frame_writer(control_frame_data){}

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

			if(value != (xe_slice<const byte>)options().accept)
				failure = true;
		}

		return 0;
	}

	int pretransfer(){
		if(follow){
			bodyless = true;
			connection_close = true;

			return xe_http_singleconnection::pretransfer();
		}

		if(failure || !connection_upgrade_seen || !upgrade_websocket_seen || !websocket_accept_seen) return XE_WEBSOCKET_CONNECTION_REFUSED;
		if(call(&xe_websocket_callbacks::ready, *request)) return XE_ABORTED;

		message_state = WS_FRAME_HEADER_FIRST;
		bodyless = false;
		is_first_fragment = true;

		return xe_http_singleconnection::pretransfer();
	}

	int predata(){
		if(is_control) return 0;
		if(payload_len > max_message_size) return XE_WEBSOCKET_MESSAGE_TOO_LONG;
		if(max_message_size - current_message_data.size() < payload_len) return XE_WEBSOCKET_MESSAGE_TOO_LONG;
		if(!current_message_data.grow(current_message_data.size() + payload_len, max_message_size)) return XE_ENOMEM;

		return 0;
	}

	int write_frame(xe_bptr buf, size_t len){
		if(is_control) control_frame_writer.write(buf, len);
		else current_message_data.append(buf, len);

		payload_len -= len;

		if(payload_len)
			return 0;
		message_state = WS_FRAME_HEADER_FIRST;

		if(is_control){
			if(opcode == WS_CLOSE){

			}else{
				auto slice = control_frame_data.slice(0, control_frame_writer.count());

				if(call(&xe_websocket_callbacks::ping, *request, opcode == WS_PING ? XE_WEBSOCKET_PING : XE_WEBSOCKET_PONG, (const xe_slice<const byte>&)slice)) return XE_ABORTED;
			}
		}else{
			if(!is_fin) return 0;
			if(call(&xe_websocket_callbacks::message, *request, (xe_websocket_op)current_message_opcode, current_message_data)) return XE_ABORTED;

			current_message_data.resize(0);
			is_first_fragment = true;
		}

		return 0;
	}

	int write_body(xe_bptr buf, size_t len){
		size_t wlen;
		byte rsv, mask;

		while(len){
			switch(message_state){
				case WS_FRAME_HEADER_FIRST:
					is_control = false;
					is_fin = (buf[0] & 0x80) ? true : false;
					rsv = buf[0] & 0x70;
					opcode = (xe_websocket_opcode)(buf[0] & 0xf);

					if(!(opcode == WS_TEXT || opcode == WS_BINARY || opcode == WS_CLOSE || opcode == WS_PING || opcode == WS_PONG)) return XE_INVALID_RESPONSE;
					if(rsv) return XE_INVALID_RESPONSE;
					if(opcode == WS_PING || opcode == WS_PONG || opcode == WS_CLOSE){
						control_frame_writer.reset();
						is_control = true;
					}else if(is_first_fragment){
						is_first_fragment = false;
						current_message_opcode = opcode;
					}else if(opcode != WS_CONTINUATION){
						return XE_INVALID_RESPONSE;
					}

					message_state = WS_FRAME_HEADER_SECOND;
					buf++;
					len--;

					break;
				case WS_FRAME_HEADER_SECOND:
					mask = buf[0] & 0x80;
					payload_len = buf[0] & 0x7f;
					message_state = WS_FRAME_DATA;

					if(is_control && payload_len > 0x7e){
						xe_log_error(this, "control frame too large");

						return XE_INVALID_RESPONSE;
					}

					if(mask){
						xe_log_error(this, "received masked frame from server");

						return XE_INVALID_RESPONSE;
					}

					if(payload_len >= 0x7e){
						payload_len = 0;
						message_state = WS_FRAME_HEADER_LENGTH;

						if(payload_len == 0x7f) payload_len_size = 8;
						else payload_len_size = 2;
					}

					buf++;
					len--;

					break;
				case WS_FRAME_HEADER_LENGTH:
					payload_len <<= 8;
					payload_len |= buf[0];
					payload_len_size--;
					buf++;
					len--;

					if(!payload_len_size){
						xe_return_error(predata());

						message_state = WS_FRAME_DATA;
					}

					break;
				case WS_FRAME_DATA:
					wlen = xe_min(len, payload_len);

					xe_return_error(write_frame(buf, wlen));

					len -= wlen;
					buf += wlen;

					break;
			}
		}

		return 0;
	}

	bool readable(){
		return message_state == WS_NONE ? xe_http_singleconnection::readable() : message_head != null;
	}

	int writable(){
		if(message_state == WS_NONE)
			return xe_http_singleconnection::readable();
		xe_assert(message_head != null);

		message_queue& data = *message_head;
		size_t result;

		result = xe_connection::send(data.data + send_offset, data.len - send_offset);

		xe_log_trace(this, "sent %zi bytes", result);

		if(result <= 0){
			if(!result) return XE_SEND_ERROR;
			if(result == XE_EAGAIN) return 0;

			return result;
		}

		send_offset += result;

		if(send_offset == data.len){
			message_head = data.next;
			send_offset = 0;

			if(!message_head)
				message_tail = null;
			xe_dealloc(&data);
		}

		return 0;
	}

	message_queue* make_send_node(size_t len, bool priority){
		message_queue* node = (message_queue*)xe_zalloc<byte>(sizeof(message_queue) + len);

		if(!node)
			return null;
		node -> len = len;

		if(!message_head){
			message_head = node;
			message_tail = node;
		}else if(priority){
			node -> next = message_head;
			message_head = node;
		}else{
			message_tail -> next = node;
			message_tail = node;
		}

		return node;
	}

	int send(xe_websocket_opcode opcode, xe_cptr buf, size_t len, bool priority = false){
		xe_localarray<byte, 14> header;
		xe_writer writer(header);
		byte payload_len = len;
		size_t sent = 0, result;

		if(len > 0xffff)
			payload_len = 0x7f;
		else if(len > 0x7d)
			payload_len = 0x7e;
		writer.write<byte>(0x80 | opcode);
		writer.write<byte>(0x80 | payload_len);

		if(payload_len == 0x7e)
			writer.write(xe_htons(len));
		else if(payload_len == 0x7f)
			writer.write(xe_htonll(len));
		writer.write<uint>(0); /* mask */

		if(message_head && (!priority || send_offset)){
		queue:
			message_queue* node = make_send_node(len + writer.count() - sent, priority);

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

		xe_log_trace(this, "sent %zi bytes", result);

		if(result < writer.count())
			goto queue;
		result = xe_connection::send(buf, len);

		if(result <= 0){
			if(!result) return XE_SEND_ERROR;
			if(result != XE_EAGAIN) return result;

			goto queue;
		}

		sent += result;

		xe_log_trace(this, "sent %zi bytes", result);

		if(result < len)
			goto queue;
		return 0;
	}

	int ping(xe_websocket_opcode type, xe_cptr data, size_t size){
		if(size > 0x7e) return XE_EINVAL;
		return send(type, data, size, true);
	}

	void close(int error){
		message_queue* node = message_head;

		while(node){
			message_queue* next = node -> next;

			xe_dealloc(node);

			node = next;
		}

		current_message_data.free();

		xe_http_singleconnection::close(error);
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

	if((err = connection -> init(*ctx)) ||
		(secure && (err = connection -> init_ssl(ctx -> ssl()))) ||
		(err = connection -> connect(data.url.hostname(), port))){
		connection -> close(err);

		return err;
	}

	connection -> open(request);

	return 0;
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
		!data.internal_set_header("Sec-WebSocket-Key", (xe_slice<const byte>)data.key, 0))
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

int xe_websocket_data::send(xe_websocket_op type, xe_cptr data, size_t size){
	xe_websocket_connection& connection = *(xe_websocket_connection*)(((xe_websocket_data_internal*)this) -> connection);

	if(type > XE_WEBSOCKET_BINARY) return XE_EINVAL;
	return connection.send((xe_websocket_opcode)type, data, size);
}

int xe_websocket_data::ping(xe_cptr data, size_t size){
	xe_websocket_connection& connection = *(xe_websocket_connection*)(((xe_websocket_data_internal*)this) -> connection);

	return connection.ping(WS_PING, data, size);
}

int xe_websocket_data::pong(xe_cptr data, size_t size){
	xe_websocket_connection& connection = *(xe_websocket_connection*)(((xe_websocket_data_internal*)this) -> connection);

	return connection.ping(WS_PONG, data, size);
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