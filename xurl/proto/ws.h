#pragma once
#include "http_base.h"
#include "xutil/container/vector.h"

namespace xurl{

enum xe_websocket_message_type{
	XE_WEBSOCKET_TEXT = 1,
	XE_WEBSOCKET_BINARY = 2
};

enum xe_websocket_ping_type{
	XE_WEBSOCKET_PING = 0,
	XE_WEBSOCKET_PONG
};

typedef int (*xe_websocket_ready_cb)(xe_request& request);
typedef int (*xe_websocket_ping_cb)(xe_request& request, xe_websocket_ping_type type);
typedef int (*xe_websocket_message_cb)(xe_request& request, xe_websocket_message_type type, xe_vector<byte>& data);

class xe_websocket_data : public xe_http_common_data{
protected:
	xe_websocket_data();
public:
	int send(xe_websocket_message_type type, xe_cptr data, size_t size);
	int ping();
	int pong();

	void set_ready_cb(xe_websocket_ready_cb cb);
	void set_ping_cb(xe_websocket_ping_cb cb);
	void set_message_cb(xe_websocket_message_cb cb);

	~xe_websocket_data();
};

xe_protocol* xe_websocket_new(xurl_ctx& ctx);

}