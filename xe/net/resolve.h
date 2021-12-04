#pragma once
#include <netdb.h>
#include "../loop.h"
#include "../container/vector.h"
#include "../string.h"

namespace xe_net{

class xe_resolve_ctx{
private:
	struct data;

	data* priv;

	friend struct xe_resolve;
public:
	int init();
	void close();
};

struct xe_endpoint{
	xe_array<in_addr> inet;
	xe_array<in6_addr> inet6;

	void free(){
		inet.free();
		inet6.free();
	}
};

struct xe_resolve{
	xe_loop& loop;
	xe_ptr resolver;

	int pollfd;
	int tfd;

	int handle;
	int flags;

	size_t count;

	xe_resolve(xe_loop& loop);

	int init(xe_resolve_ctx& ctx);
	void close();

	int start();
	void stop();

	int resolve(xe_net_ctx& net, xe_string host, xe_endpoint& endpoint);
private:
	static void io(xe_handle&, int);

	friend struct ::xe_loop;
};

}