#pragma once
#include "../conn.h"
#include "../ctx.h"
#include "net_common.h"

namespace xurl{

static int xe_start_connection(
	xurl_ctx& ctx, xe_connection& conn, const xe_net_common_data& data,
	bool secure, const xe_string_view& hostname, uint port
){
	xe_return_error(conn.init(ctx));

	conn.set_ssl_verify(data.get_ssl_verify());
	conn.set_ip_mode(data.get_ip_mode());

	if(secure)
		xe_return_error(conn.init_ssl(data.get_ssl_ctx()));
	xe_return_error(conn.connect(hostname, port, data.get_connect_timeout()));

	return 0;
}

}