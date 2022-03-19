#pragma once
#include "../types.h"
#include "proto/net_common.h"
#include "xe/config.h"

namespace xe_net{

class xe_connection;
class xe_connection_handler{
public:
	xe_connection_handler(){};

	virtual void state_change(int state) = 0;
	virtual int ready() = 0;
	virtual int writable() = 0;
	virtual ssize_t write(xe_ptr data, size_t size) = 0;
	virtual void closed(int error) = 0;

	virtual ~xe_connection_handler(){}
};

}

#include "net.h"
#include "protocol.h"
#include "ssl.h"

namespace xe_net{

enum xe_connection_state{
	XE_CONNECTION_STATE_IDLE = 0,
	XE_CONNECTION_STATE_RESOLVING,
	XE_CONNECTION_STATE_CONNECTING,
	XE_CONNECTION_STATE_HANDSHAKE,
	XE_CONNECTION_STATE_ACTIVE,
	XE_CONNECTION_STATE_CLOSED
};

struct xe_connection{
	xe_connection* next;
	xe_connection* prev;
	xe_net_ctx* net;
	xe_connection_handler* handler;

	xe_ssl ssl;
	xe_string host;

	xe_connection_state state;

	uint endpoint_index;
	xe_endpoint endpoint;

	ushort port;
	ushort flags;
	int fd;

	uint recvbuf_size;
	byte ip_mode;

#ifdef XE_DEBUG
	ulong time;
#endif
	void init(xe_net_ctx& net, xe_connection_handler& data);
	void set_connect_timeout(uint timeout_ms);
	void set_ip_mode(xe_ip_mode mode);
	void set_ssl_verify(bool verify);
	void set_recvbuf_size(uint size);

	int init_ssl(xe_ssl_ctx& ctx);

	int connect(xe_string host, int port);
	ssize_t send(xe_cptr data, size_t size);

	int poll_writable(bool poll);

	int pause(bool paused);
	int keepalive();

	void close(int error);
	void close();
private:
	void io(int);
	void resolved(xe_endpoint&, int status);

	friend struct xe_net_ctx;
};

}