#pragma once
#include "../string.h"

namespace xe_net{

struct xe_request;

}

#include "protocol.h"

namespace xe_net{

enum xe_request_state{
	XE_REQUEST_STATE_IDLE = 0,
	XE_REQUEST_STATE_RESOLVING,
	XE_REQUEST_STATE_CONNECTING,
	XE_REQUEST_STATE_ACTIVE,
	XE_REQUEST_STATE_COMPLETE
};

enum xe_request_callback{
	XE_REQUEST_CALLBACK_NONE = 0,
	XE_REQUEST_CALLBACK_STATE,
	XE_REQUEST_CALLBACK_WRITE,
	XE_REQUEST_CALLBACK_DONE,
	XE_REQUEST_CALLBACK_LAST
};

struct xe_request{
	xe_request_state state;
	xe_protocol_data* data;

	struct xe_callbacks{
		void (*state)(xe_request& request, xe_request_state state);
		int (*write)(xe_request& request, xe_ptr buf, size_t size);
		void (*done)(xe_request& request, int error);
	} callbacks;

	void set_state(xe_request_state);
	bool write(xe_ptr, size_t);
	void finished(int);

	~xe_request();

	int set_callback(int option, xe_ptr callback);

	template<class T>
	int set_callback(int option, T callback){
		return set_callback(option, (xe_ptr)callback);
	}

	int set_http_method(xe_string method, int flags = 0);
	int set_http_header(xe_string key, xe_string value, int flags = 0);
};

}