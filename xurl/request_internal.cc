#include "request_internal.h"

using namespace xurl;

void xe_request_internal::set_state(xe_request_state state){
	state_ = state;

	if(callbacks.state) callbacks.state(*this, state);
}

int xe_request_internal::write(xe_ptr buf, size_t size){
	return callbacks.write ? callbacks.write(*this, buf, size) : 0;
}

void xe_request_internal::complete(int error){
	set_state(XE_REQUEST_STATE_COMPLETE);

	if(callbacks.done) callbacks.done(*this, error);
}