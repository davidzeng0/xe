#pragma once
#include <netdb.h>
#include "xe/loop.h"
#include "xstd/types.h"
#include "xstd/slice.h"
#include "xstd/string.h"
#include "xurl.h"

namespace xurl{

class xe_resolve_ctx{
private:
	struct xe_resolve_ctx_data;

	xe_resolve_ctx_data* priv;

	friend class xe_resolve;
public:
	int init();
	void close();
};

class xe_endpoint{
private:
	xe_slice<in_addr> inet_;
	xe_slice<in6_addr> inet6_;

	friend class xe_resolve;
public:
	xe_endpoint(){}

	xe_endpoint(const xe_endpoint& other) = delete;
	xe_endpoint& operator=(const xe_endpoint& other) = delete;

	xe_endpoint(xe_endpoint&& other);
	xe_endpoint& operator=(xe_endpoint&& other);

	const xe_slice<in_addr>& inet() const;
	const xe_slice<in6_addr>& inet6() const;

	void free();

	~xe_endpoint();
};

class xe_resolve{
private:
	xurl_ctx* ctx;
	xe_timer timer;
	xe_ptr resolver;

	int pollfd;
	size_t count;

	void io();

	static void sockstate(xe_ptr, int, int, int);
	static int sockcreate(int, int, xe_ptr);
	static void resolved(xe_ptr, int, int, xe_ptr);

	static int ip_resolve(const xe_string&, xe_endpoint&);
	static int timeout(xe_loop&, xe_timer&);

	friend class xurl_ctx;
public:
	xe_resolve();

	int init(xurl_ctx& xurl_ctx, xe_resolve_ctx& ctx);
	void close();

	int resolve(const xe_string& host, xe_endpoint& endpoint);

	static xe_cstr class_name();
};

}