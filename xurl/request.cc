#include "xutil/mem.h"
#include "request.h"

using namespace xurl;

xe_request_state xe_request::state(){
	return state_;
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

void xe_request::set_ssl_ctx(const xe_ssl_ctx& ctx){
	((xe_net_common_data*)data) -> set_ssl_ctx(ctx);
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

void xe_request::set_max_redirects(uint max_redirects){
	((xe_http_common_data*)data) -> set_max_redirects(max_redirects);
}

void xe_request::set_follow_location(bool follow){
	((xe_http_common_data*)data) -> set_follow_location(follow);
}

int xe_request::set_http_header(const xe_string_view& key, const xe_string_view& value, uint flags){
	return ((xe_http_specific*)data) -> set_header(key, value, flags);
}

int xe_request::set_http_method(const xe_string_view& method, uint flags){
	return ((xe_http_specific*)data) -> set_method(method, flags);
}

void xe_request::set_http_min_version(xe_http_version version){
	return ((xe_http_specific*)data) -> set_min_version(version);
}

void xe_request::set_http_max_version(xe_http_version version){
	return ((xe_http_specific*)data) -> set_max_version(version);
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

void xe_request::set_http_trailer_cb(xe_http_singleheader_cb cb){
	((xe_http_specific*)data) -> set_trailer_cb(cb);
}

int xe_request::ws_send(xe_websocket_op op, xe_cptr buf, size_t size){
	return ((xe_websocket_data*)data) -> send(op, buf, size);
}

int xe_request::ws_ping(xe_cptr buf, size_t size){
	return ((xe_websocket_data*)data) -> ping(buf, size);
}

int xe_request::ws_pong(xe_cptr buf, size_t size){
	return ((xe_websocket_data*)data) -> pong(buf, size);
}

int xe_request::ws_close(ushort code, xe_cptr buf, size_t size){
	return ((xe_websocket_data*)data) -> close(code, buf, size);
}

void xe_request::set_ws_ready_cb(xe_websocket_ready_cb cb){
	((xe_websocket_data*)data) -> set_ready_cb(cb);
}

void xe_request::set_ws_ping_cb(xe_websocket_ping_cb cb){
	((xe_websocket_data*)data) -> set_ping_cb(cb);
}

void xe_request::set_ws_message_cb(xe_websocket_message_cb cb){
	((xe_websocket_data*)data) -> set_message_cb(cb);
}

void xe_request::set_ws_close_cb(xe_websocket_close_cb cb){
	((xe_websocket_data*)data) -> set_close_cb(cb);
}

void xe_request::close(){
	xe_deletep(data);
}