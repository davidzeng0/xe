#pragma once
#include "xstd/types.h"
#include "xconfig/config.h"
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
	void io(int);
	void start_connect(xe_endpoint&, int);

	static int io(xe_connection&, int);
	static int create_socket(xe_connection&, int);
	static int try_connect(xe_connection&);
	static int ready(xe_connection&);
	static int socket_read(xe_connection&);

	friend class xurl_ctx;

	xe_connection* next;
	xe_connection* prev;
	xurl_ctx* ctx;
	xe_ptr buf;

	xe_ssl ssl;
#ifdef XE_DEBUG
	xe_string_view host;
	ulong time;
#endif
	xe_endpoint* endpoint;
	size_t endpoint_index;

	xe_ip_mode ip_mode;
	int fd;

	ushort port;
	bool ssl_enabled: 1;
	bool ssl_verify: 1;
#ifdef XE_DEBUG
	bool refcounted: 1;
#endif
protected:
	bool recv_paused: 1;
	bool send_paused: 1;

	xe_connection_state state;
	xe_timer timer;

	static int timeout(xe_loop&, xe_timer&);

	int set_nodelay(bool nodelay);
	int set_recvbuf_size(int size);
	int set_keepalive(bool enable, int idle);
	int shutdown(uint flags);
	int start_timer(ulong ms, uint flags = 0);
	int stop_timer();

	virtual int init_socket();
	virtual void set_state(xe_connection_state state);
	virtual int ready();
	virtual bool readable();
	virtual int writable();
	virtual ssize_t data(xe_ptr data, size_t size) = 0;
public:
	xe_connection(){}

	int init(xurl_ctx& ctx);
	void set_ip_mode(xe_ip_mode mode);
	void set_ssl_verify(bool verify);

	int init_ssl(xe_ssl_ctx& ctx);

	void start_connect_timeout(uint timeout_ms);
	int connect(const xe_string_view& host, int port);
	ssize_t send(xe_cptr data, size_t size);

	int transferctl(uint flags);
	bool peer_closed();

	virtual void close(int error);

	static xe_cstr class_name();
};

}