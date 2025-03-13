#pragma once
#include <netdb.h>
#include "xe/loop.h"
#include "xe/io/poll.h"
#include "xstd/types.h"
#include "xstd/list.h"
#include "xstd/string.h"
#include "xstd/rbtree.h"
#include "xutil/util.h"

namespace xurl{

class xe_endpoint;
typedef void (*xe_resolve_cb)(xe_ptr user, const xe_string_view& host, xe_endpoint&& endpoint, int status);

class xe_resolve_ctx{
private:
	struct xe_resolve_ctx_data;

	xe_resolve_ctx_data* priv;

	friend class xe_resolve;
public:
	xe_resolve_ctx() = default;

	int init();
	void close();

	~xe_resolve_ctx() = default;
};

class xe_endpoint{
private:
	xe_slice<in_addr> inet_;
	xe_slice<in6_addr> inet6_;

	friend class xe_resolve;
public:
	xe_endpoint() = default;

	xe_disable_copy(xe_endpoint)

	xe_endpoint(xe_endpoint&& other);
	xe_endpoint& operator=(xe_endpoint&& other);

	const xe_slice<in_addr>& inet() const;
	const xe_slice<in6_addr>& inet6() const;

	void clear();

	~xe_endpoint();
};

class xe_resolve_node : public xe_rb_node{
public:
	int fd;

	xe_resolve_node(): fd(){}
	xe_resolve_node(int fd): fd(fd){}

	bool operator<(const xe_resolve_node& o) const{
		return fd < o.fd;
	}

	bool operator>(const xe_resolve_node& o) const{
		return fd > o.fd;
	}

	bool operator==(const xe_resolve_node& o) const{
		return fd == o.fd;
	}

	~xe_resolve_node() = default;
};

class xe_resolve;
struct xe_resolve_handle{
	xe_resolve_node node;
	xe_poll poll;
	xe_resolve* resolve;
};

class xe_resolve{
private:
	static void poll_cb(xe_poll&, int);
	static void close_cb(xe_poll&);

	static void sockstate(xe_ptr, int, int, int);
	static int sockcreate(int, int, xe_ptr);
	static void resolved(xe_ptr, int, int, xe_ptr);
	static int timeout(xe_loop&, xe_timer&);

	int ip_resolve(const xe_string&, xe_endpoint&);

	xe_loop* loop_;
	xe_timer timer;
	xe_ptr resolver;

	xe_rbtree<xe_resolve_node> handles;

	bool closing: 1;
public:
	void (*close_callback)(xe_resolve& resolve);

	xe_resolve(){
		timer.callback = timeout;
		closing = false;
		close_callback = null;
	}

	xe_disable_copy_move(xe_resolve)

	xe_loop& loop() const{
		return *loop_;
	}

	int init(xe_loop& loop, const xe_resolve_ctx& ctx);
	int close();

	int resolve(const xe_string& host, xe_endpoint& endpoint, xe_resolve_cb callback, xe_ptr user);

	~xe_resolve() = default;

	static xe_cstr class_name();
};

}