#pragma once
#include "xutil/string.h"
#include "xurl.h"
#include "proto/http.h"
#include "proto/ws.h"

namespace xurl{

enum xe_request_state{
	XE_REQUEST_STATE_IDLE = 0,
	XE_REQUEST_STATE_DNS,
	XE_REQUEST_STATE_CONNECTING,
	XE_REQUEST_STATE_ACTIVE,
	XE_REQUEST_STATE_COMPLETE
};

class xe_request{
public:
	xe_protocol_specific* data;
protected:
	typedef void (*state_cb)(xe_request& request, xe_request_state state);
	typedef int (*write_cb)(xe_request& request, xe_ptr buf, size_t size);
	typedef void (*done_cb)(xe_request& request, int error);

	struct xe_callbacks{
		state_cb state;
		write_cb write;
		done_cb done;
	} callbacks;

	xe_request_state state;

	friend class xurl_ctx;
public:
	xe_request();

	void set_state_cb(state_cb cb);
	void set_write_cb(write_cb cb);
	void set_done_cb(done_cb cb);

	void set_port(ushort port);
	void set_connect_timeout(uint timeout_ms);
	void set_ssl_verify(bool verify);
	void set_ip_mode(xe_ip_mode mode);
	void set_recvbuf_size(uint size);

	void set_max_redirects(uint max_redirects);
	void set_follow_location(bool follow);
	int set_http_header(const xe_string_view& key, const xe_string_view& value, bool copy = false);

	int set_http_method(const xe_string_view& method, bool copy = false);
	void set_http_min_version(xe_http_version version);
	void set_http_max_version(xe_http_version version);
	void set_http_statusline_cb(xe_http_statusline_cb cb);
	void set_http_singleheader_cb(xe_http_singleheader_cb cb);
	void set_http_response_cb(xe_http_response_cb cb);
	void set_http_trailer_cb(xe_http_singleheader_cb cb);

	int ws_send(xe_websocket_message_type type, xe_cptr data, size_t size);
	int ws_ping();
	int ws_pong();
	void set_ws_ready_cb(xe_websocket_ready_cb cb);
	void set_ws_ping_cb(xe_websocket_ping_cb cb);
	void set_ws_message_cb(xe_websocket_message_cb cb);

	void close();

	~xe_request();
};

}