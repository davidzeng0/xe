#include "xe/error.h"
#include "../ssl_common.h"

using namespace xurl;

int xe_ssl_ctx::init(){
	return XE_ENOSYS;
}

int xe_ssl_ctx::load_default_verify_locations(){
	return XE_ENOSYS;
}

int xe_ssl_ctx::load_verify_locations(xe_cstr cafile, xe_cstr capath){
	return XE_ENOSYS;
}

void xe_ssl_ctx::close(){}

int xe_ssl::init(const xe_ssl_ctx& ctx){
	return XE_ENOSYS;
}

void xe_ssl::close(){}

int xe_ssl::preconnect(int fd){
	return XE_ENOSYS;
}

int xe_ssl::verify_host(const xe_string& host){
	return XE_ENOSYS;
}

int xe_ssl::set_alpn(const xe_string_view& protocols){
	return XE_ENOSYS;
}

int xe_ssl::connect(int flags){
	return XE_ENOSYS;
}

int xe_ssl::get_alpn_protocol(xe_string_view& proto){
	return XE_ENOSYS;
}

int xe_ssl::recv(xe_ptr buffer, size_t len, int flags){
	return XE_ENOSYS;
}

int xe_ssl::send(xe_cptr buffer, size_t len, int flags){
	return XE_ENOSYS;
}

int xe_crypto::sha1(const byte* in, size_t inlen, byte* out, size_t outlen){
	return XE_ENOSYS;
}

int xurl::xe_ssl_init(){
	return XE_ENOSYS;
}

void xurl::xe_ssl_cleanup(){}