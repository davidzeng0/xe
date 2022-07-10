#include "request.h"

using namespace xurl;

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

void xe_request::complete(int error){
	set_state(XE_REQUEST_STATE_COMPLETE);

	if(callbacks.done)
		callbacks.done(*this, error);
}

void xe_request::set_state_cb(state_cb cb){
	callbacks.state = cb;
}

void xe_request::set_write_cb(write_cb cb){
	callbacks.write = cb;
}

void xe_request::set_done_cb(done_cb cb){
	callbacks.done = cb;
}

void xe_request::set_port(ushort port){
	((xe_net_common_data*)data) -> set_port(port);
}

void xe_request::set_connect_timeout(uint timeout_ms){
	((xe_net_common_data*)data) -> set_connect_timeout(timeout_ms);
}

void xe_request::set_ssl_verify(bool verify){
	((xe_net_common_data*)data) -> set_ssl_verify(verify);
}

void xe_request::set_ip_mode(xe_ip_mode mode){
	((xe_net_common_data*)data) -> set_ip_mode(mode);
}

void xe_request::set_recvbuf_size(uint size){
	((xe_net_common_data*)data) -> set_recvbuf_size(size);
}

int xe_request::set_http_method(xe_string method, bool copy){
	return ((xe_http_specific*)data) -> set_method(method, copy);
}

int xe_request::set_http_header(xe_string key, xe_string value, bool copy){
	return ((xe_http_specific*)data) -> set_header(key, value, copy);
}

void xe_request::set_max_redirects(uint max_redirects){
	((xe_http_common_data*)data) -> set_max_redirects(max_redirects);
}

void xe_request::set_follow_location(bool follow){
	((xe_http_common_data*)data) -> set_follow_location(follow);
}

void xe_request::set_http_statusline_cb(xe_http_statusline_cb cb){
	((xe_http_specific*)data) -> set_statusline_cb(cb);
}

void xe_request::set_http_singleheader_cb(xe_http_singleheader_cb cb){
	((xe_http_specific*)data) -> set_singleheader_cb(cb);
}

void xe_request::set_http_response_cb(xe_http_response_cb cb){
	((xe_http_specific*)data) -> set_response_cb(cb);
}

void xe_request::close(){
	if(data){
		xe_delete(data);

		data = null;
	}
}