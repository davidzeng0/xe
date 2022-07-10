#pragma once
#include "xe/string.h"
#include "xurl.h"
#include "proto/http.h"

namespace xurl{

enum xe_request_state{
	XE_REQUEST_STATE_IDLE = 0,
	XE_REQUEST_STATE_DNS,
	XE_REQUEST_STATE_CONNECTING,
	XE_REQUEST_STATE_ACTIVE,
	XE_REQUEST_STATE_COMPLETE
};

struct xe_request{
	typedef void (*state_cb)(xe_request& request, xe_request_state state);
	typedef int (*write_cb)(xe_request& request, xe_ptr buf, size_t size);
	typedef void (*done_cb)(xe_request& request, int error);

	xe_protocol_specific* data;

	struct xe_callbacks{
		state_cb state;
		write_cb write;
		done_cb done;
	} callbacks;

	xe_request_state state;

	void set_state(xe_request_state);
	bool write(xe_ptr, size_t);
	void complete(int);

	void set_state_cb(state_cb cb);
	void set_write_cb(write_cb cb);
	void set_done_cb(done_cb cb);

	void set_port(ushort port);
	void set_connect_timeout(uint timeout_ms);
	void set_ssl_verify(bool verify);
	void set_ip_mode(xe_ip_mode mode);
	void set_recvbuf_size(uint size);

	int set_http_method(xe_string method, bool copy = false);
	int set_http_header(xe_string key, xe_string value, bool copy = false);
	void set_max_redirects(uint max_redirects);
	void set_follow_location(bool follow);
	void set_http_statusline_cb(xe_http_statusline_cb cb);
	void set_http_singleheader_cb(xe_http_singleheader_cb cb);
	void set_http_response_cb(xe_http_response_cb cb);

	void close();

	~xe_request();
};

}