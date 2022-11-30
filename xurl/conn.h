#pragma once
#include "xconfig/config.h"
#include "xstd/types.h"
#include "xutil/util.h"
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

class xe_connection : protected xe_linked_node{
private:
	static void poll_cb(xe_poll&, int);
	static void close_cb(xe_poll&);

	static int io(xe_connection&, int);
	static int create_socket(xe_connection&, int);
	static int try_connect(xe_connection&);
	static int ready(xe_connection&);
	static int socket_read(xe_connection&);

	void start_connect(const xe_shared_ref<xe_endpoint>& endpoint_);

#ifdef XE_DEBUG
	xe_string_view host;
	ulong time;
#endif

	xe_connection_ctx* ctx;

	xe_poll poll;
	xe_ptr buf;
	xe_ssl ssl;

	xe_shared_ref<xe_endpoint> endpoint;
	size_t ip_index;
	xe_ip_mode ip_mode;

	int fd;

	ushort port;
	bool ssl_enabled: 1;
	bool ssl_verify: 1;

	friend class xe_connection_ctx;
protected:
	static int timeout(xe_loop&, xe_timer&);

	int set_nodelay(bool nodelay);
	int set_recvbuf_size(int size);
	int set_keepalive(bool enable, int idle);
	int shutdown(uint flags);
	int start_timer(ulong ms, uint flags = 0);
	int stop_timer();
	ssize_t send(xe_cptr data, size_t size);

	virtual int init_socket();
	virtual void set_state(xe_connection_state state);
	virtual int ready();
	virtual bool readable();
	virtual int writable();
	virtual ssize_t data(xe_ptr data, size_t size) = 0;
	virtual void closed();

	bool recv_paused: 1;
	bool send_paused: 1;

	xe_connection_state state;
	xe_timer timer;
public:
	xe_connection(){
		poll.poll_callback = poll_cb;
		poll.close_callback = close_cb;

		ip_index = 0;
		ip_mode = XE_IP_ANY;

		ssl_enabled = false;
		ssl_verify = false;
		recv_paused = false;
		send_paused = false;

		state = XE_CONNECTION_STATE_IDLE;
	}

	xe_disable_copy_move(xe_connection)

	int init(xurl_ctx& ctx);
	int init_ssl(const xe_ssl_ctx& ctx);

	void set_ip_mode(xe_ip_mode mode);
	void set_ssl_verify(bool verify);

	int connect(const xe_string_view& host, ushort port, uint timeout_ms = 0);

	int transferctl(uint flags);
	bool peer_closed();

	virtual void close(int error);
	virtual ~xe_connection();

	static xe_cstr class_name();
};

}