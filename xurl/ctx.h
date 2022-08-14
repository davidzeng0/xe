#pragma once
#include "xstd/types.h"
#include "xstd/unique_ptr.h"
#include "xe/loop.h"
#include "xstd/map.h"
#include "resolve.h"
#include "protocol.h"
#include "xurl.h"
#include "ssl.h"

namespace xurl{

class xurl_shared{
private:
	xe_resolve_ctx resolve;
	xe_ptr protocol_data[XE_PROTOCOL_LAST];
	xe_map<xe_string, xe_endpoint> endpoints;
	xe_ssl_ctx* ssl_ctx;

	friend class xurl_ctx;
public:
	int init(xe_ssl_ctx& ssl);

	xe_ssl_ctx& ssl();

	void close();
};

class xurl_ctx{
private:
	struct resolve_entry{
		xe_connection* null;
		xe_connection* pending;
		xe_endpoint endpoint;
	};

	xe_loop* loop_;
	xurl_shared* shared;

	xe_resolve resolver;
	xe_protocol* protocols[XE_PROTOCOL_LAST];

	xe_connection* connections;
	size_t conn_count;

	uint active: 1;
	uint closing: 1;
	uint flags: 30;

	int pollfd;
	int eventfd;
	int handle;

	xe_map<xe_string, xe_unique_ptr<resolve_entry>> endpoints;

	int resolve(xe_connection&, const xe_string_view&, xe_endpoint*&);
	void resolver_active(bool);

	int poll(xe_connection&, int, int, int);

	bool count(xe_connection&);
	void uncount(xe_connection&);
	void add(xe_connection&);
	void resolve_remove(xe_connection&);
	void remove(xe_connection&);

	void resolved(const xe_string_view&, xe_endpoint&&, int);
	void poll();

	static void io(xe_loop_handle&, int);

	friend class ::xe_loop;
	friend class xe_resolve;
	friend class xe_connection;
public:
	xurl_ctx();

	int init(xe_loop& loop, xurl_shared& shared);
	void close();

	int start();
	void stop();

	int open(xe_request& request, const xe_string_view& url);
	int start(xe_request& request);
	int transferctl(xe_request& request, uint flags);
	int end(xe_request& request);

	xe_loop& loop();
	xe_ssl_ctx& ssl();

	size_t connection_count();

	static xe_cstr class_name();
};

}