#include "request.h"
#include "proto/http.h"

using namespace xe_net;

xe_request::~xe_request(){
	if(data)
		xe_delete(data);
}

void xe_request::set_state(xe_request_state _state){
	state = _state;

	if(callbacks.state)
		callbacks.state(*this, _state);
}

bool xe_request::write(xe_ptr buf, size_t size){
	if(callbacks.write && callbacks.write(*this, buf, size))
		return false;
	return true;
}

void xe_request::finished(int error){
	set_state(XE_REQUEST_STATE_COMPLETE);

	if(callbacks.done)
		callbacks.done(*this, error);
}

int xe_request::set_callback(int option, xe_ptr callback){
	switch(option){
		case XE_REQUEST_CALLBACK_STATE:
			*(xe_ptr*)&callbacks.state = callback;

			break;
		case XE_REQUEST_CALLBACK_WRITE:
			*(xe_ptr*)&callbacks.write = callback;

			break;
		case XE_REQUEST_CALLBACK_DONE:
			*(xe_ptr*)&callbacks.done = callback;

			break;
		default:
			return data -> set(option, callback, 0);
	}

	return 0;
}

int xe_request::set_http_method(xe_string method, int flags){
	return data -> set(XE_HTTP_METHOD, &method, flags);
}

int xe_request::set_http_header(xe_string key, xe_string value, int flags){
	return data -> set(XE_HTTP_HEADER, &key, &value, flags);
}