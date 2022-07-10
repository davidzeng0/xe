#pragma once
#include "http_base.h"

namespace xurl{

typedef int (*xe_websocket_ready_cb)(xe_request& request);
typedef int (*xe_websocket_ping_cb)(xe_request& request);

enum xe_websocket_message_type{
	XE_WEBSOCKET_TEXT = 1,
	XE_WEBSOCKET_BINARY = 2
};

class xe_websocket_data : public xe_http_common_data{
protected:
	xe_websocket_data();
public:
	int send(xe_ptr data, size_t size);
	int ping();
	int pong();

	~xe_websocket_data();
};

xe_protocol* xe_websocket_new(xurl_ctx& ctx);

}