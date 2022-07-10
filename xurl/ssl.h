#pragma once
#include "xe/common.h"
#include "xe/error.h"
#include "xe/string.h"

namespace xurl{

class xe_ssl_ctx{
private:
	xe_ptr data;

	friend class xe_ssl;
public:
	int init();
	int load_verify_locations(xe_cstr cafile, xe_cstr capath);
	void close();
};

class xe_ssl{
private:
	xe_ptr data;
public:
	int init(xe_ssl_ctx& ctx);
	void close();

	void set_fd(int fd);
	int verify_host(xe_string host);
	int set_alpn(xe_string protocols);

	int connect(int flags);
	int get_alpn_protocol(xe_string& proto);

	int recv(xe_ptr buffer, size_t len, int flags);
	int send(xe_cptr buffer, size_t len, int flags);
};

int xe_ssl_init();
void xe_ssl_cleanup();

}