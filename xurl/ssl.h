#pragma once
#include "xstd/string.h"

namespace xurl{

class xe_ssl_ctx{
private:
	xe_ptr data;

	friend class xe_ssl;
public:
	xe_ssl_ctx(): data(){}

	xe_disallow_copy_move(xe_ssl_ctx)

	int init();
	int load_default_verify_locations();
	int load_verify_locations(xe_cstr cafile, xe_cstr capath);
	void close();

	~xe_ssl_ctx() = default;
};

class xe_ssl{
private:
	xe_ptr data;
public:
	xe_ssl(): data(){}

	xe_disallow_copy_move(xe_ssl)

	int init(const xe_ssl_ctx& ctx);
	void close();

	void set_fd(int fd);
	int verify_host(const xe_string_view& host);
	int set_alpn(const xe_string_view& protocols);

	int connect(int flags);
	int get_alpn_protocol(xe_string_view& proto);

	int recv(xe_ptr buffer, size_t len, int flags);
	int send(xe_cptr buffer, size_t len, int flags);

	~xe_ssl() = default;
};

int xe_ssl_init();
void xe_ssl_cleanup();

}