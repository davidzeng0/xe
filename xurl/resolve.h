#pragma once
#include <netdb.h>
#include "xe/loop.h"
#include "xutil/container/vector.h"
#include "xutil/string.h"
#include "xurl.h"

namespace xurl{

class xe_resolve_ctx{
private:
	struct xe_resolve_ctx_data;

	xe_resolve_ctx_data* priv;

	friend struct xe_resolve;
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

	const xe_slice<const in_addr>& inet() const;
	const xe_slice<const in6_addr>& inet6() const;

	void free();

	~xe_endpoint();
};

class xe_resolve{
private:
	xe_ptr resolver;
	xe_loop* loop;
	xurl_ctx* ctx;

	int pollfd;
	int tfd;

	uint handle;
	uint active: 1;
	uint flags: 31;

	size_t count;

	static void io(xe_loop_handle&, int);
	static void sockstate(xe_ptr, int, int, int);
	static int sockcreate(int, int, xe_ptr);
	static void resolved(xe_ptr, int, int, xe_ptr);
	static int ip_resolve(const xe_string_view&, xe_endpoint&);
	void poll();

	friend struct ::xe_loop;
public:
	xe_resolve();

	int init(xurl_ctx& xurl_ctx, xe_resolve_ctx& ctx);
	void close();

	int start();
	void stop();

	int resolve(const xe_string_view& host, xe_endpoint& endpoint);

	static xe_cstr class_name();
};

}