#pragma once
#include "xstd/types.h"
#include "xstd/unique_ptr.h"
#include "xe/loop.h"
#include "xstd/map.h"
#include "resolve.h"
#include "protocol.h"
#include "xurl.h"
#include "ssl.h"
#include "xstd/fla.h"

namespace xurl{

class xurl_shared{
private:
	xe_map<xe_string, xe_endpoint> endpoints;
	xe_fla<xe_ptr, XE_PROTOCOL_LAST> protocol_data;

	xe_resolve_ctx resolve_ctx_;
	xe_ssl_ctx ssl_ctx_;
public:
	xurl_shared(){
		for(xe_ptr& ptr : protocol_data)
			ptr = null;
	}

	xe_disallow_copy_move(xurl_shared)

	int init();
	void close();

	const xe_resolve_ctx& resolve_ctx();
	const xe_ssl_ctx& ssl_ctx();

	~xurl_shared() = default;
};

class xurl_ctx{
private:
	static void resolved(xe_ptr, const xe_string_view&, xe_endpoint&&, int);

	int resolve(xe_connection&, const xe_string_view&, xe_endpoint*&);

	void add(xe_connection&);
	void resolve_remove(xe_connection&);
	void remove(xe_connection&);
	void count();
	void uncount();

	struct xe_resolve_entry{
		xe_connection* null;
		xe_connection* pending;
		xe_endpoint endpoint;
	};

	xe_loop* loop_;
	xurl_shared* shared;

	xe_fla<xe_protocol*, XE_PROTOCOL_LAST> protocols;
	xe_map<xe_string, xe_unique_ptr<xe_resolve_entry>> endpoints;

	xe_resolve resolver;

	xe_connection* connections;
	size_t active_connections_;
	size_t active_resolves_;

	bool closing: 1;

	friend class xe_connection;
public:
	xurl_ctx(){
		for(auto& protocol : protocols)
			protocol = null;
		connections = null;
		active_connections_ = 0;
		active_resolves_ = 0;

		closing = false;
	}

	xe_disallow_copy_move(xurl_ctx)

	int init(xe_loop& loop, xurl_shared& shared);
	void close();

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
		return active_connections_;
	}

	size_t active_resolves() const{
		return active_resolves_;
	}

	~xurl_ctx() = default;

	static xe_cstr class_name();
};

}