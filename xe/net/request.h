#pragma once
#include "../string.h"

namespace xe_net{

struct xe_request;

}

#include "protocol.h"
#include "proto/http.h"

namespace xe_net{

enum xe_request_state{
	XE_REQUEST_STATE_IDLE = 0,
	XE_REQUEST_STATE_RESOLVING,
	XE_REQUEST_STATE_CONNECTING,
	XE_REQUEST_STATE_ACTIVE,
	XE_REQUEST_STATE_COMPLETE
};

struct xe_request{
	xe_request_state state;
	xe_protocol_data* data;

	typedef void (*state_cb)(xe_request& request, xe_request_state state);
	typedef int (*write_cb)(xe_request& request, xe_ptr buf, size_t size);
	typedef void (*done_cb)(xe_request& request, int error);

	struct xe_callbacks{
		state_cb state;
		write_cb write;
		done_cb done;
	} callbacks;

	void set_state(xe_request_state);
	bool write(xe_ptr, size_t);
	void finished(int);

	~xe_request();

	int set_state_cb(state_cb cb);
	int set_write_cb(write_cb cb);
	int set_done_cb(done_cb cb);

	int set_port(ushort port);
	int set_connect_timeout(uint timeout_ms);
	int set_max_redirects(uint max_redirects);
	int set_ssl_verify(bool verify);
	int set_follow_location(bool follow);
	int set_ip_mode(xe_ip_mode mode);
	int set_recvbuf_size(uint size);

	int set_http_method(xe_string method, bool clone = false);
	int set_http_header(xe_string key, xe_string value, bool clone = false);
	int set_http_response_cb(xe_http_response_cb cb);
	int set_http_header_cb(xe_http_header_cb cb);

	void close();
};

}