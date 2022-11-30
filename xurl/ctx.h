#pragma once
#include "xstd/types.h"
#include "xstd/map.h"
#include "xstd/fla.h"
#include "xstd/unique_ptr.h"
#include "xstd/shared_ptr.h"
#include "xstd/linked_list.h"
#include "xutil/util.h"
#include "xe/loop.h"
#include "protocol.h"
#include "resolve.h"
#include "xurl.h"
#include "ssl.h"

namespace xurl{

class xurl_shared{
private:
	xe_resolve_ctx resolve_ctx_;
	xe_ssl_ctx ssl_ctx_;
public:
	xurl_shared() = default;

	xe_disable_copy_move(xurl_shared)

	int init();
	void close();

	const xe_resolve_ctx& resolve_ctx();
	const xe_ssl_ctx& ssl_ctx();

	~xurl_shared() = default;
};

class xe_connection_ctx{
private:
	void resolved(const xe_shared_ref<xe_endpoint>&, xe_linked_list<xe_connection>&, int);
	void close();
	bool closing();

	friend class xurl_ctx;
public:
	xe_linked_list<xe_connection> list;
	xe_linked_list<xe_connection> close_pending;
	size_t active; /* connections that are currently in the event loop */

	xe_connection_ctx(): active(){}

	xe_loop& loop();

	int resolve(xe_connection&, const xe_string_view&, xe_shared_ref<xe_endpoint>&);
	void add(xe_connection&);
	void closing(xe_connection&);
	void remove(xe_connection&);

	~xe_connection_ctx() = default;
};

struct xe_resolve_entry : public xe_linked_node{
	xe_shared_ref<xe_endpoint> endpoint;
	xe_linked_list<xe_connection> pending;
	xe_string_view key;
	ulong time;
	bool in_progress: 1;

	xe_resolve_entry(xe_shared_ref<xe_endpoint>&&);
	~xe_resolve_entry() = default;
};

class xurl_ctx{
private:
	static void resolved(xe_ptr, const xe_string_view&, xe_endpoint&&, int);
	static void close_cb(xe_resolve&);
	static void expire_cb(xe_loop&, xe_timer&);

	int alloc_entry(const xe_string_view&, xe_map<xe_string, xe_unique_ptr<xe_resolve_entry>>::iterator&);
	void start_expire_timer();
	void resolve_success(xe_resolve_entry&);
	void purge_expired();
	int resolve(xe_connection&, const xe_string_view&, xe_shared_ref<xe_endpoint>&, xe_linked_list<xe_connection>*&);
	void check_close();

	xe_loop* loop_;
	xurl_shared* shared;

	xe_fla<xe_unique_ptr<xe_protocol>, XE_PROTOCOL_LAST> protocols;
	xe_map<xe_string, xe_unique_ptr<xe_resolve_entry>> endpoints;

	xe_linked_list<xe_resolve_entry> expire;
	xe_timer expire_timer;

	xe_connection_ctx connections;
	xe_resolve resolver;
	size_t active_resolves_;

	bool closing: 1;
	bool resolver_closing: 1;

	friend class xe_connection_ctx;
	friend class xe_connection;
public:
	void (*close_callback)(xurl_ctx& ctx);

	xurl_ctx(){
		resolver.close_callback = close_cb;
		active_resolves_ = 0;

		closing = false;
		resolver_closing = false;
		close_callback = null;
	}

	xe_disable_copy_move(xurl_ctx)

	int init(xe_loop& loop, xurl_shared& shared);
	int close();

	int open(xe_request& request, const xe_string_view& url);
	int start(xe_request& request);
	int transferctl(xe_request& request, uint flags);
	int end(xe_request& request);

	xe_loop& loop() const{
		return *loop_;
	}

	const xe_ssl_ctx& ssl_ctx() const{
		return shared -> ssl_ctx();
	}

	size_t active_connections() const{
		return connections.active;
	}

	size_t active_resolves() const{
		return active_resolves_;
	}

	~xurl_ctx() = default;

	static xe_cstr class_name();
};

}