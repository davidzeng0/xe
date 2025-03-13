#include "http_internal.h"
#include "net_internal.h"
#include "xutil/endian.h"
#include "xutil/encoding.h"
#include "xutil/log.h"
#include "xstd/fla.h"
#include "xe/clock.h"
#include "ws.h"
#include "xutil/writer.h"
#include "../random.h"

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

	~xe_websocket() = default;

	static xe_cstr class_name(){
		return "xe_websocket";
	}
};

struct xe_websocket_callbacks{
	xe_websocket_ready_cb ready;
	xe_websocket_ping_cb ping;
	xe_websocket_message_cb message;
	xe_websocket_close_cb close;
};

class xe_websocket_connection;
class xe_websocket_data_internal : public xe_websocket_data, public xe_http_internal_data{
protected:
	friend class xe_websocket;
public:
	xe_websocket_callbacks callbacks;

	xe_fla<byte, 24> key;
	xe_fla<byte, 28> accept;
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
	CONTROL_FRAME_MAX_LENGTH = 0x7d,
	SHORT_FRAME_LENGTH = 0x7e,
	LONG_FRAME_LENGTH = 0x7f,
	SHORT_FRAME_MIN_LENGTH = 0x7e,
	LONG_FRAME_MIN_LENGTH = 0x1000
};

enum{
	MAX_MESSAGE_SIZE = 100 * 1024 * 1024,
	CLOSE_TIMEOUT = 30 * 1000
};

struct message_queue{
	message_queue* next;
	size_t len;
	byte data[];
};

class xe_websocket_connection : public xe_http_singleconnection{
protected:
	bool connection_upgrade_seen: 1;
	bool upgrade_websocket_seen: 1;
	bool websocket_accept_seen: 1;
	bool failure: 1;

	bool is_first_fragment: 1;
	bool is_control: 1;
	bool is_fin: 1;

	bool closing: 1;
	bool close_received: 1;
	bool in_callback: 1;

	uint message_state;
	uint payload_len_size;
	ulong payload_len;
	xe_websocket_opcode opcode;

	xe_fla<byte, CONTROL_FRAME_MAX_LENGTH> control_frame_data;
	size_t control_frame_length;

	xe_websocket_opcode current_message_opcode;
	xe_vector<byte> current_message_data;

	ulong max_message_size;

	message_queue* message_head;
	message_queue* message_tail;
	size_t send_offset;

	xe_websocket_data_internal& options(){
		return *(xe_websocket_data_internal*)specific;
	}

	template<typename F, typename... Args>
	bool call(F xe_websocket_callbacks::*field, Args&& ...args){
		auto callback = options().callbacks.*field;

		return callback && callback(std::forward<Args>(args)...) ? true : false;
	}

	static int timeout(xe_loop& loop, xe_timer& timer){
		xe_websocket_connection& conn = (xe_websocket_connection&)xe_containerof(timer, &xe_websocket_connection::timer);

		conn.close(0);

		return XE_ECANCELED;
	}

	bool readable(){
		return message_state == WS_NONE ? xe_http_singleconnection::readable() : message_head != null;
	}

	int writable(){
		if(message_state == WS_NONE)
			return xe_http_singleconnection::writable();
		xe_assert(message_head != null);

		message_queue& data = *message_head;
		ssize_t result;

		result = xe_connection::send(data.data + send_offset, data.len - send_offset);

		if(result <= 0){
			if(!result) return XE_SEND_ERROR;
			if(result == XE_EAGAIN) return 0;

			return result;
		}

		send_offset += result;

		if(send_offset == data.len){
			message_head = data.next;
			send_offset = 0;

			xe_dealloc(&data);

			if(!message_head){
				message_tail = null;

				if(closing && close_received) return finish_close();
			}
		}

		return 0;
	}

	int handle_status_line(xe_http_version version, uint status, const xe_string_view& reason){
		if(status != 101){
			failure = true;

			xe_log_error(this, "status code mismatch");
		}

		return 0;
	}

	int handle_header(const xe_string_view& key, const xe_string_view& value){
		xe_return_error(xe_http_singleconnection::handle_header(key, value));

		if(failure)
			return 0;
		if(key.equal_case("Connection")){
			if(value.equal_case("upgrade"))
				connection_upgrade_seen = true;
			else{
				failure = true;

				xe_log_error(this, "connection header mismatch");
			}
		}else if(key.equal_case("Upgrade")){
			if(value.equal_case("websocket"))
				upgrade_websocket_seen = true;
			else{
				failure = true;

				xe_log_error(this, "upgrade header mismatch");
			}
		}else if(key.equal_case("Sec-WebSocket-Accept")){
			websocket_accept_seen = true;

			if(value != xe_string_view((char*)options().accept.data(), options().accept.size())){
				failure = true;

				xe_log_error(this, "accept header mismatch");
			}
		}

		return 0;
	}

	int pretransfer(){
		if(follow){
			bodyless = true;
			connection_close = true;

			return xe_http_singleconnection::pretransfer();
		}

		if(failure || !connection_upgrade_seen || !upgrade_websocket_seen || !websocket_accept_seen){
			xe_log_error(this, "websocket connection refused");

			return XE_WEBSOCKET_CONNECTION_REFUSED;
		}

		if(call(&xe_websocket_callbacks::ready, *request))
			return XE_ECANCELED;
		message_state = WS_FRAME_HEADER_FIRST;
		bodyless = false;
		is_first_fragment = true;

		return xe_http_singleconnection::pretransfer();
	}

	int predata(){
		message_state = WS_FRAME_DATA;

		if(is_control)
			return 0;
		if(payload_len > max_message_size)
			return XE_WEBSOCKET_MESSAGE_TOO_LONG;
		if(max_message_size - current_message_data.size() < payload_len)
			return XE_WEBSOCKET_MESSAGE_TOO_LONG;
		if(!current_message_data.grow(current_message_data.size() + payload_len, max_message_size))
			return XE_ENOMEM;
		return 0;
	}

	void write_control_frame(byte* buf, size_t len){
		xe_assert(len < control_frame_data.size() - control_frame_length);
		xe_memcpy(control_frame_data.data() + control_frame_length, buf, len);

		control_frame_length += len;
	}

	int write_frame(byte* buf, size_t len){
		if(is_control) write_control_frame(buf, len);
		else current_message_data.append(buf, len);

		payload_len -= len;

		if(payload_len)
			return 0;
		message_state = WS_FRAME_HEADER_FIRST;

		if(is_control){
			if(opcode == WS_CLOSE){
				ushort code;
				uint start = 0;

				close_received = true;

				if(control_frame_length >= 2){
					start = 2;
					code = xe_ntoh(*(ushort*)control_frame_data.data());
				}else{
					code = 1005;
				}

				if(call(&xe_websocket_callbacks::close, *request, code, control_frame_data.slice(start, control_frame_length)))
					return XE_ECANCELED;
				if(closing && !message_head)
					return finish_close();
				close(code, null, 0);
			}else if(call(&xe_websocket_callbacks::ping, *request, opcode == WS_PING ? XE_WEBSOCKET_PING : XE_WEBSOCKET_PONG, control_frame_data.slice(0, control_frame_length))){
				return XE_ECANCELED;
			}
		}else{
			if(!is_fin)
				return 0;
			if(call(&xe_websocket_callbacks::message, *request, (xe_websocket_op)current_message_opcode, current_message_data))
				return XE_ECANCELED;
			current_message_data.resize(0);
			is_first_fragment = true;
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
			node -> next = message_head -> next;
			message_head -> next = node;
		}else{
			message_tail -> next = node;
			message_tail = node;
		}

		return node;
	}

	int ws_transfer(byte* buf, size_t len){
		size_t wlen;
		byte rsv, mask;

		while(len){
			switch(message_state){
				case WS_FRAME_HEADER_FIRST:
					is_control = false;
					is_fin = (buf[0] & 0x80) ? true : false;
					rsv = buf[0] & 0x70;
					opcode = (xe_websocket_opcode)(buf[0] & 0xf);

					if(!(opcode == WS_CONTINUATION || opcode == WS_TEXT || opcode == WS_BINARY || opcode == WS_CLOSE || opcode == WS_PING || opcode == WS_PONG))
						return XE_EPROTO;
					if(rsv)
						return XE_EPROTO;
					if(opcode == WS_PING || opcode == WS_PONG || opcode == WS_CLOSE){
						control_frame_length = 0;
						is_control = true;

						if(!is_fin) return XE_EPROTO;
					}else if(is_first_fragment){
						is_first_fragment = false;
						current_message_opcode = opcode;
					}else if(opcode != WS_CONTINUATION){
						return XE_EPROTO;
					}

					message_state = WS_FRAME_HEADER_SECOND;
					buf++;
					len--;

					break;
				case WS_FRAME_HEADER_SECOND:
					mask = buf[0] & 0x80;
					payload_len = buf[0] & 0x7f;

					if(is_control && payload_len > CONTROL_FRAME_MAX_LENGTH){
						xe_log_error(this, "control frame too large");

						return XE_EPROTO;
					}

					if(mask){
						xe_log_error(this, "received masked frame from server");

						return XE_EPROTO;
					}

					if(payload_len >= SHORT_FRAME_LENGTH){
						payload_len = 0;
						message_state = WS_FRAME_HEADER_LENGTH;

						if(payload_len == LONG_FRAME_LENGTH)
							payload_len_size = 8;
						else
							payload_len_size = 2;
					}else{
						xe_return_error(predata());
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

					if(!payload_len_size)
						xe_return_error(predata());
					break;
				case WS_FRAME_DATA:
					wlen = xe_min(len, payload_len);

					xe_return_error(write_frame(buf, wlen));

					len -= wlen;
					buf += wlen;

					if(close_received && len)
						return XE_ECANCELED;
					break;
			}
		}

		return 0;
	}

	int write_body(byte* buf, size_t len){
		int err;

		if(close_received)
			return XE_ECANCELED;
		in_callback = true;
		err = ws_transfer(buf, len);
		in_callback = false;

		return err;
	}

	int finish_close(){
		complete(0);

		return xe_http_singleconnection::shutdown(SHUT_WR);
	}
public:
	xe_websocket_connection(xe_websocket& proto): xe_http_singleconnection(proto){
		max_message_size = MAX_MESSAGE_SIZE;
	}

	int send(xe_websocket_opcode opcode, xe_cptr buf, size_t len, bool priority = false){
		xe_fla<byte, 14> header;
		xe_writer writer(header);
		byte payload_len = len;
		ssize_t result;
		size_t sent;

		if(state != XE_CONNECTION_STATE_ACTIVE || closing)
			return XE_STATE;
		if(opcode == WS_CLOSE)
			closing = true;
		if(len >= LONG_FRAME_MIN_LENGTH)
			payload_len = LONG_FRAME_LENGTH;
		else if(len >= SHORT_FRAME_MIN_LENGTH)
			payload_len = SHORT_FRAME_LENGTH;
		writer.w8(0x80 | opcode);
		writer.w8(0x80 | payload_len);

		if(payload_len == SHORT_FRAME_LENGTH)
			writer.w16be(len);
		else if(payload_len == LONG_FRAME_LENGTH)
			writer.w64be(len);
		writer.w32be(0); /* mask */
		sent = 0;

		if(message_head)
			goto queue;
		result = xe_connection::send(header.data(), writer.pos());

		if(result <= 0){
			if(!result) return XE_SEND_ERROR;
			if(result != XE_EAGAIN) return result;

			goto queue;
		}

		sent += result;

		if((size_t)result < writer.pos())
			goto queue;
		result = xe_connection::send(buf, len);

		if(result <= 0){
			if(!result) return XE_SEND_ERROR;
			if(result != XE_EAGAIN) return result;

			goto queue;
		}

		sent += result;

		if((size_t)result < len)
			goto queue;
		if(closing){
			if(close_received)
				return finish_close();
			timer.callback = timeout;

			start_timer(CLOSE_TIMEOUT);
		}

		return 0;
	queue:
		message_queue* node;

		if(!message_head)
			transferctl(XE_RESUME_SEND);
		node = make_send_node(len + writer.pos() - sent, priority);

		if(!node)
			return XE_ENOMEM;
		if(sent < writer.pos()){
			xe_memcpy(node -> data, header.data() + sent, writer.pos() - sent);
			xe_memcpy(node -> data + writer.pos() - sent, buf, len);
		}else{
			xe_memcpy(node -> data, buf, len + writer.pos() - sent);
		}

		return 0;
	}

	int ping(xe_websocket_opcode type, xe_cptr data, size_t size){
		if(size > CONTROL_FRAME_MAX_LENGTH) return XE_EINVAL;
		return send(type, data, size, true);
	}

	void close(int error){
		message_queue* node = message_head;

		while(node){
			message_queue* next = node -> next;

			xe_dealloc(node);

			node = next;
		}

		current_message_data.clear();

		xe_http_singleconnection::close(error);
	}

	void closed(){
		xe_http_singleconnection::closed();

		xe_delete(this);
	}

	int close(ushort code, xe_cptr data, size_t size){
		xe_fla<byte, CONTROL_FRAME_MAX_LENGTH> body;
		xe_writer writer(body);

		if(size > CONTROL_FRAME_MAX_LENGTH - 2)
			return XE_EINVAL;
		if(closing)
			return XE_EALREADY;
		writer.w16be(code);
		writer.write(data, size);

		return send(WS_CLOSE, body.data(), writer.pos(), false);
	}

	void end(xe_request_internal& req){
		close(XE_ECANCELED);
	}

	xe_cstr class_name(){
		return "xe_websocket_connection";
	}
};

xe_websocket::xe_websocket(xurl_ctx& ctx): xe_http_protocol(ctx, XE_PROTOCOL_WEBSOCKET){
	uint seed;

	if(xe_crypto_random(&seed, sizeof(seed)) == sizeof(seed))
		random.seed(seed);
	else
		random.seed(xe_realtime_ms());
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

		xe_log_verbose(this, "using default port %u", port);
	}

	xe_websocket_connection* connection = xe_znew<xe_websocket_connection>(*this);

	if(!connection)
		return XE_ENOMEM;
	err = xe_start_connection(*ctx, *connection, data, secure, data.url.hostname(), port);

	if(err)
		connection -> close(err);
	else
		connection -> open(request);
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

		if(!data)
			return XE_ENOMEM;
		data -> ssl_ctx = &ctx -> ssl_ctx();
	}

	if((err = open(*data, std::move(url), false))){
		if(data != request.data)
			xe_delete(data);
		return err;
	}

	request.data = data;

	xe_log_verbose(this, "opened ws request for: %s", data -> url.href().data());

	return 0;
}

int xe_websocket::open(xe_websocket_data_internal& data, xe_url&& url, bool redirect){
	xe_fla<byte, 16> rng;
	xe_fla<byte, 20> sum;
	xe_fla<byte, 60> accept;
	xe_writer writer(accept);

	random.random_bytes(rng.data(), rng.size());

	xe_base64_encode(XE_BASE64_PAD, data.key.data(), data.key.size(), rng.data(), rng.size());

	writer.write(data.key);
	writer.write("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

	xe_return_error(xe_crypto::sha1(accept.data(), accept.size(), sum.data(), sum.size()));
	xe_base64_encode(XE_BASE64_PAD, data.accept.data(), data.accept.size(), sum.data(), sum.size());
	xe_return_error(xe_http_protocol::open(data, std::move(url), redirect));

	if(!data.internal_set_header("Connection", "Upgrade", 0) ||
		!data.internal_set_header("Upgrade", "websocket", 0) ||
		!data.internal_set_header("Sec-WebSocket-Version", "13", 0) ||
		!data.internal_set_header("Sec-WebSocket-Key", xe_string_view((char*)data.key.data(), data.key.size()), 0))
		return XE_ENOMEM;
	return 0;
}

bool xe_websocket::matches(const xe_string_view& scheme) const{
	return scheme == "ws" || scheme == "wss";
}

void xe_websocket::redirect(xe_request_internal& request, xe_string&& url){
	xe_log_verbose(this, "redirect to %.*s", url.length(), url.data());

	int err = internal_redirect(request, std::move(url));

	if(err) request.complete(err);
}

int xe_websocket::internal_redirect(xe_request_internal& request, xe_string&& url){
	xe_websocket_data_internal& data = *(xe_websocket_data_internal*)request.data;
	xe_url parser(std::move(url));

	if(data.redirects++ >= data.max_redirects)
		return XE_TOO_MANY_REDIRECTS;
	xe_return_error(parser.parse());

	if(!matches(parser.scheme())){
		data.url = std::move(parser);

		return XE_EXTERNAL_REDIRECT;
	}

	xe_return_error(open(data, std::move(parser), true));

	return start(request);
}

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

int xe_websocket_data::close(ushort code, xe_cptr data, size_t size){
	xe_websocket_connection& connection = *(xe_websocket_connection*)(((xe_websocket_data_internal*)this) -> connection);

	return connection.close(code, data, size);
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

void xe_websocket_data::set_close_cb(xe_websocket_close_cb cb){
	xe_websocket_data_internal& internal = *(xe_websocket_data_internal*)this;

	internal.callbacks.close = cb;
}

xe_websocket_data::~xe_websocket_data(){}

xe_protocol* xurl::xe_websocket_new(xurl_ctx& ctx){
	return xe_new<xe_websocket>(ctx);
}