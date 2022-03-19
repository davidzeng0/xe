#include "request.h"

using namespace xe_net;

xe_request::~xe_request(){
	if(data)
		xe_delete(data);
}

void xe_request::set_state(xe_request_state state_){
	state = state_;

	if(callbacks.state)
		callbacks.state(*this, state_);
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

int xe_request::set_state_cb(state_cb cb){
	callbacks.state = cb;

	return 0;
}

int xe_request::set_write_cb(write_cb cb){
	callbacks.write = cb;

	return 0;
}

int xe_request::set_done_cb(done_cb cb){
	callbacks.done = cb;

	return 0;
}

int xe_request::set_port(ushort port){
	return data -> set(XE_NET_PORT, (xe_ptr)(ulong)port, 0);
}

int xe_request::set_connect_timeout(uint timeout_ms){
	return data -> set(XE_NET_CONNECT_TIMEOUT, (xe_ptr)(ulong)timeout_ms, 0);
}

int xe_request::set_max_redirects(uint max_redirects){
	return data -> set(XE_NET_MAX_REDIRECT, (xe_ptr)(ulong)max_redirects, 0);
}

int xe_request::set_ssl_verify(bool verify){
	return data -> set(XE_NET_SSL_VERIFY, (xe_ptr)verify, 0);
}

int xe_request::set_follow_location(bool follow){
	return data -> set(XE_NET_FOLLOW_LOCATION, (xe_ptr)follow, 0);
}

int xe_request::set_ip_mode(xe_ip_mode mode){
	return data -> set(XE_NET_IP_MODE, (xe_ptr)(ulong)mode, 0);
}

int xe_request::set_recvbuf_size(uint size){
	return data -> set(XE_NET_RECVBUF_SIZE, (xe_ptr)(ulong)size, 0);
}

int xe_request::set_http_method(xe_string method, bool clone){
	return data -> set(XE_HTTP_METHOD, &method, clone ? XE_HTTP_CLONE_STRING : 0);
}

int xe_request::set_http_header(xe_string key, xe_string value, bool clone){
	return data -> set(XE_HTTP_HEADER, &key, &value, clone ? XE_HTTP_CLONE_STRING : 0);
}

int xe_request::set_http_response_cb(xe_http_response_cb cb){
	return data -> set(XE_HTTP_CALLBACK_RESPONSE, (xe_ptr)cb, 0);
}

int xe_request::set_http_header_cb(xe_http_header_cb cb){
	return data -> set(XE_HTTP_CALLBACK_HEADER, (xe_ptr)cb, 0);
}

void xe_request::close(){
	if(data){
		xe_delete(data);

		data = null;
	}
}