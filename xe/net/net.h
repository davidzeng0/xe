#pragma once
#include "../types.h"
#include "../loop.h"
#include "../container/map.h"

namespace xe_net{

struct xe_net_ctx;

}

#include "resolve.h"
#include "protocol.h"
#include "request.h"
#include "conn.h"
#include "ssl.h"

namespace xe_net{

class xe_net_shared{
private:
	xe_resolve_ctx resolve;
	xe_ssl_ctx ssl;
	xe_map<xe_string, xe_endpoint> endpoints;

	friend struct xe_net_ctx;
public:
	int init();
	int init_ssl(xe_cstr cafile, xe_cstr capath);

	void free();
};

struct xe_net_ctx{
	xe_resolve resolver;
	xe_loop* loop;
	xe_ssl_ctx* ssl_ctx;
	xe_protocol* protocols[XE_PROTOCOL_LAST];

	xe_connection* connections;
	ulong conn_count;

	int flags;

	int pollfd;
	int handle;

	struct endpoint{
		xe_endpoint endpoint;
		xe_connection* pending;
	};

	xe_map<xe_string, endpoint> endpoints;

	xe_net_ctx();

	int resolve(xe_connection& conn, xe_string& host, xe_endpoint& endpoint);
	int poll(xe_connection&, int, int, int);
	void count();
	void uncount();
	void add(xe_connection& conn);
	void remove(xe_connection& conn);

	void resolved(xe_string, xe_endpoint, int);

	int init(xe_loop& loop, xe_net_shared& shared);
	void destroy();

	int start();
	void stop();

	int open(xe_request& request, xe_string url);
	int start(xe_request& request);
	int pause(xe_request& request, bool pause);
	int end(xe_request& request);

	xe_loop& get_loop();
	xe_ssl_ctx& get_ssl_ctx();
private:
	static void io(xe_loop_handle&, int);

	friend class ::xe_loop;
};

int xe_net_init();
void xe_net_cleanup();

}