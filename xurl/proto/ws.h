#pragma once
#include "xstd/types.h"
#include "xstd/vector.h"
#include "http_base.h"

namespace xurl{

enum xe_websocket_op{
	XE_WEBSOCKET_TEXT = 0x1,
	XE_WEBSOCKET_BINARY = 0x2,
	XE_WEBSOCKET_PING = 0x9,
	XE_WEBSOCKET_PONG = 0xa,
};

typedef int (*xe_websocket_ready_cb)(xe_request& request);
typedef int (*xe_websocket_ping_cb)(xe_request& request, xe_websocket_op op, xe_slice<byte> data);
typedef int (*xe_websocket_message_cb)(xe_request& request, xe_websocket_op op, xe_vector<byte>& data);
typedef int (*xe_websocket_close_cb)(xe_request& request, ushort code, xe_slice<byte> data);
typedef int (*xe_websocket_begin_fragment_cb)(xe_request& request, ulong len);
typedef int (*xe_websocket_write_fragment_cb)(xe_request& request, xe_ptr data, size_t len);

class xe_websocket_data : public xe_http_common_data{
protected:
	xe_websocket_data();
public:
	void set_handshake_timeout(uint handshake_timeout);
	uint get_handshake_timeout();

	int send(xe_websocket_op op, xe_cptr data, size_t size);
	int ping(xe_cptr data, size_t size);
	int pong(xe_cptr data, size_t size);
	int close(ushort code, xe_cptr data, size_t size);

	void set_ready_cb(xe_websocket_ready_cb cb);
	void set_ping_cb(xe_websocket_ping_cb cb);
	void set_message_cb(xe_websocket_message_cb cb);
	void set_close_cb(xe_websocket_close_cb cb);

	~xe_websocket_data();
};

xe_protocol* xe_websocket_new(xurl_ctx& ctx);

}