#include "ws.h"
#include "http_internal.h"
#include "../random.h"
#include "../common.h"
#include "../conn.h"
#include "../request.h"
#include "../encoding.h"

using namespace xurl;

class xe_websocket : public xe_http_protocol{
public:
	xe_rng random;

	xe_websocket(xurl_ctx& ctx);

	int start(xe_request& request);

	int transferctl(xe_request& request, uint flags);
	void end(xe_request& request);

	int open(xe_request& request, xe_url url);

	bool matches(const xe_string& scheme) const;

	void redirect(xe_request& request, xe_string&& url);
	int internal_redirect(xe_request& request, xe_string&& url);
	bool available(xe_http_connection& connection, bool available);
	int open(xe_http_internal_data& data, xe_url url, bool redirect);

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
	} callbacks;
};

enum xe_websocket_opcode{
	WS_CONTINUATION = 0x0,
	WS_TEXT = 0x1,
	WS_BINARY = 0x2,
	WS_CLOSE = 0x8,
	WS_PING = 0x9,
	WS_PONG = 0xA
};

class xe_websocket_connection : public xe_http_singleconnection{
protected:
	bool connection_upgrade_seen: 1;
	bool upgrade_websocket_seen: 1;
	bool failure: 1;
public:
	xe_websocket_connection(xe_websocket& proto): xe_http_singleconnection(proto){}

	int handle_statusline(xe_http_version version, uint status, xe_string& reason){
		if(status != 101)
			failure = true;\
		return 0;
	}

	int handle_singleheader(xe_string& key, xe_string& value){
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
		}

		return 0;
	}

	int write_body(xe_bptr buf, size_t len){
		if(failure || !connection_upgrade_seen || !upgrade_websocket_seen)
			return XE_WEBSOCKET_CONNECTION_REFUSED;
		return ws_data(buf, len);
	}

	enum{
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

	byte fin, rsv, opcode;
	byte mask;
	uint ws_state;
	ulong payload_len;

	int ws_data(xe_bptr buf, size_t len){
		size_t wlen;

		while(len){
			switch(ws_state){
				case WS_FRAME_HEADER_FIRST:
					fin = buf[0] & 0x80;
					rsv = buf[0] & 0x70;
					opcode = buf[0] & 0xf;
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

					client_write(buf, wlen);

					len -= wlen;
					buf += wlen;
					payload_len -= wlen;

					if(!payload_len)
						ws_state = WS_FRAME_HEADER_FIRST;
					break;
			}
		}

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

int xe_websocket::start(xe_request& request){
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
	connection -> open(request);

	do{
		if((err = connection -> init(*ctx)))
			break;
		if(secure && (err = connection -> init_ssl(ctx -> ssl())))
			break;
		if((err = connection -> connect(data.url.hostname(), port)))
			break;
		return 0;
	}while(false);

	xe_delete(connection);

	return err;
}

int xe_websocket::transferctl(xe_request& request, uint flags){
	xe_websocket_data_internal& data = *(xe_websocket_data_internal*)request.data;

	return data.connection -> transferctl(request, flags);
}

void xe_websocket::end(xe_request& request){
	xe_websocket_data_internal& data = *(xe_websocket_data_internal*)request.data;

	data.connection -> end(request);
}

int xe_websocket::open(xe_request& request, xe_url url){
	xe_websocket_data_internal* data;

	int err;

	if(request.data && request.data -> id() == XE_PROTOCOL_WEBSOCKET)
		data = (xe_websocket_data_internal*)request.data;
	else{
		data = xe_znew<xe_websocket_data_internal>();

		if(!data) return XE_ENOMEM;
	}

	if((err = open(*(xe_http_internal_data*)data, url, false))){
		if(data != request.data)
			xe_delete(data);
		return err;
	}

	xe_log_verbose(this, "opened ws request for: %s", url.string().c_str());

	request.data = data;

	return 0;
}

int xe_websocket::open(xe_http_internal_data& data, xe_url url, bool redirect){
	byte rng[16], key[24];

	xe_return_error(xe_http_protocol::open(data, url, redirect));

	random.random_bytes(rng, sizeof(rng));

	xe_base64_encode(XE_BASE64_PAD, key, sizeof(key), rng, sizeof(rng));

	if(!data.internal_set_header("Connection", "Upgrade", 0) ||
		!data.internal_set_header("Upgrade", "websocket", 0) ||
		!data.internal_set_header("Sec-WebSocket-Version", "13", 0) ||
		!data.internal_set_header("Sec-WebSocket-Key", key, XE_HTTP_COPY_VALUE))
		return XE_ENOMEM;
	return 0;
}

bool xe_websocket::matches(const xe_string& scheme) const{
	return scheme == "ws" || scheme == "wss";
}

void xe_websocket::redirect(xe_request& request, xe_string&& url){
	int err = internal_redirect(request, std::forward<xe_string>(url));

	if(err) request.complete(err);
}

int xe_websocket::internal_redirect(xe_request& request, xe_string&& url){
	xe_websocket_data_internal& data = *(xe_websocket_data_internal*)request.data;
	xe_url parser(url);

	if(data.redirects++ >= data.max_redirects)
		return XE_TOO_MANY_REDIRECTS;
	xe_return_error(parser.parse());

	if(!matches(parser.scheme()))
		return XE_EXTERNAL_REDIRECT;
	xe_return_error(open((xe_http_internal_data&)data, parser, true));

	return start(request);
}

bool xe_websocket::available(xe_http_connection& connection, bool available){
	return true;
}

xe_websocket::~xe_websocket(){}

xe_websocket_data::xe_websocket_data(): xe_http_common_data(XE_PROTOCOL_WEBSOCKET){}

xe_websocket_data::~xe_websocket_data(){}

xe_protocol* xurl::xe_websocket_new(xurl_ctx& ctx){
	return xe_new<xe_websocket>(ctx);
}