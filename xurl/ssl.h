#pragma once
#include "xstd/types.h"
#include "xstd/string.h"
#include "xutil/util.h"

namespace xurl{

class xe_ssl_ctx{
private:
	xe_ptr data;

	friend class xe_ssl;
public:
	xe_ssl_ctx(): data(){}

	xe_disable_copy_move(xe_ssl_ctx)

	int init();
	int load_default_verify_locations();
	int load_verify_locations(xe_cstr cafile, xe_cstr capath);
	void close();

	~xe_ssl_ctx() = default;

	static xe_cstr class_name();
};

class xe_ssl{
private:
	xe_ptr data;
public:
	xe_ssl(): data(){}

	xe_disable_copy_move(xe_ssl)

	int init(const xe_ssl_ctx& ctx);
	void close();

	int verify_host(const xe_string& host);
	int set_alpn(const xe_string_view& protocols);

	int preconnect(int fd);
	int connect(int flags);
	int get_alpn_protocol(xe_string_view& proto);

	int recv(xe_ptr buffer, size_t len, int flags);
	int send(xe_cptr buffer, size_t len, int flags);

	~xe_ssl() = default;

	static xe_cstr class_name();
};

class xe_crypto{
public:
	static int sha1(const byte* in, size_t inlen, byte* out, size_t outlen);
};

int xe_ssl_init();
void xe_ssl_cleanup();

}