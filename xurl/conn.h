#pragma once
#include "xe/types.h"
#include "proto/net_common.h"
#include "xe/config.h"
#include "protocol.h"
#include "ssl.h"
#include "ctx.h"

namespace xurl{

enum xe_connection_state{
	XE_CONNECTION_STATE_IDLE = 0,
	XE_CONNECTION_STATE_RESOLVING,
	XE_CONNECTION_STATE_CONNECTING,
	XE_CONNECTION_STATE_HANDSHAKE,
	XE_CONNECTION_STATE_ACTIVE,
	XE_CONNECTION_STATE_CLOSED
};

class xe_connection{
private:
	xe_connection* next;
	xe_connection* prev;
	xurl_ctx* ctx;
	xe_ptr buf;

	xe_ssl ssl;

	uint endpoint_index;
	xe_endpoint* endpoint;

	int fd;

	uint recvbuf_size;
	xe_ip_mode ip_mode;

	ushort port;
	ushort ssl_enabled: 1;
	ushort ssl_verify: 1;
	ushort recv_paused: 1;
	ushort send_paused: 1;
	ushort refcounted: 1;
	ushort tcp_keepalive: 1;
#ifdef XE_DEBUG
	xe_string host;
	ulong time;
#endif
	void io(int);
	void start_connect(xe_endpoint&, int);

	static int io(xe_connection&, int);
	static int create_socket(xe_connection&, int);
	static int try_connect(xe_connection&);
	static int ready(xe_connection&);
	static int socket_read(xe_connection&);

	friend class xurl_ctx;
protected:
	xe_connection_state state;

	virtual void set_state(xe_connection_state state);
	virtual int ready();
	virtual bool readable();
	virtual int writable();
	virtual ssize_t data(xe_ptr data, size_t size) = 0;
public:
	xe_connection();

	int init(xurl_ctx& ctx);
	void set_connect_timeout(uint timeout_ms);
	void set_ip_mode(xe_ip_mode mode);
	void set_ssl_verify(bool verify);
	void set_recvbuf_size(uint size);
	void set_tcp_keepalive(bool keepalive);

	int init_ssl(xe_ssl_ctx& ctx);

	int connect(xe_string host, int port);
	ssize_t send(xe_cptr data, size_t size);

	int transferctl(uint flags);
	bool peer_closed();

	virtual void close(int error);

	~xe_connection();

	static xe_cstr class_name();
};

}