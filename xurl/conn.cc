#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "xutil/endian.h"
#include "xutil/log.h"
#include "xe/clock.h"
#include "xe/error.h"
#include "conn.h"

using namespace xurl;

enum{
	XE_CONNECTION_MSG_FLAGS = MSG_NOSIGNAL
};

void xe_connection::poll_cb(xe_poll& poll, int res){
	xe_connection& conn = xe_containerof(poll, &xe_connection::poll);

	if(res >= 0) [[likely]]
		res = io(conn, res);
	if(res) conn.close(res);
}

void xe_connection::close_cb(xe_poll& poll){
	xe_connection& conn = xe_containerof(poll, &xe_connection::poll);

	conn.closed();
}

int xe_connection::io(xe_connection& conn, int res){
	socklen_t len = sizeof(res);

	switch(conn.state){
		case XE_CONNECTION_STATE_CONNECTING:
			if(getsockopt(conn.fd, SOL_SOCKET, SO_ERROR, &res, &len) < 0)
				return xe_errno();
			if(res){
				res = xe_syserror(res);

				xe_log_debug(&conn, "connection failed, try %zu in %.3f ms, status: %s", conn.ip_index + 1, (xe_time_ns() - conn.time) / (float)XE_NANOS_PER_MS, xe_strerror(res));

				if(res != XE_ECONNREFUSED && res != XE_ETIMEDOUT)
					return res;
				if(conn.ip_index < xe_max_value(conn.ip_index)){
					conn.ip_index++;

					return try_connect(conn);
				}

				return res;
			}

			conn.endpoint.free();

			xe_log_verbose(&conn, "connected to %.*s:%u after %zu tries in %.3f ms", conn.host.length(), conn.host.data(), xe_ntoh(conn.port), (size_t)conn.ip_index + 1, (xe_time_ns() - conn.time) / (float)XE_NANOS_PER_MS);
			xe_return_error(conn.init_socket());

			if(conn.ssl_enabled){
				xe_return_error(conn.poll.poll(XE_POLL_IN));
				xe_return_error(conn.ssl.preconnect(conn.fd));

				conn.set_state(XE_CONNECTION_STATE_HANDSHAKE);

			#ifdef XE_DEBUG
				conn.time = xe_time_ns();
			#endif
				/* fallthrough: send ssl hello */
			}else{
				xe_return_error(ready(conn));

				break;
			}
		case XE_CONNECTION_STATE_HANDSHAKE:
			res = conn.ssl.connect(XE_CONNECTION_MSG_FLAGS);

			if(!res){
				xe_log_verbose(&conn, "ssl connected in %.3f ms", (xe_time_ns() - conn.time) / (float)XE_NANOS_PER_MS);

				return ready(conn);
			}else if(res != XE_EAGAIN){
				return res;
			}

			break;
		case XE_CONNECTION_STATE_ACTIVE:
			if(res & XE_POLL_OUT) [[unlikely]] {
				/* send data */
				xe_return_error(conn.writable());

				if(!conn.readable())
					xe_return_error(conn.transferctl(XE_PAUSE_SEND));
			}

			if(res & XE_POLL_IN)
				return socket_read(conn);
		default:
			xe_notreached();

			break;
	}

	return 0;
}

void xe_connection::start_connect(const xe_shared_ref<xe_endpoint>& endpoint_){
	/* name resolution completed asynchronously */
	int err;

	endpoint = endpoint_;

	if((err = try_connect(*this)))
		close(err);
	else{
		set_state(XE_CONNECTION_STATE_CONNECTING);

		ctx -> active++;
	}
}

int xe_connection::socket_read(xe_connection& conn){
	xe_ptr buf = conn.buf;
	ssize_t result;

	for(uint n = 0; n < 64; n++){
		if(conn.ssl_enabled)
			result = conn.ssl.recv(buf, XE_LOOP_IOBUF_SIZE, 0);
		else{
			result = recv(conn.fd, buf, XE_LOOP_IOBUF_SIZE, 0);

			if(result < 0) result = xe_errno();
		}

		if(result < 0){
			if(result == XE_EAGAIN)
				return 0;
			xe_log_trace(&conn, ">> connection %zi (%s)", result, xe_strerror(result));

			return result;
		}

		xe_log_trace(&conn, ">> connection %zi", result);

		result = conn.data(buf, result);

		if(result <= 0){
			conn.close(result);

			break;
		}

		if(conn.recv_paused) break;
	}

	return 0;
}

int xe_connection::timeout(xe_loop& loop, xe_timer& timer){
	xe_connection& conn = xe_containerof(timer, &xe_connection::timer);

	conn.close(XE_ETIMEDOUT);

	return XE_ECANCELED;
}

int xe_connection::create_socket(xe_connection& conn, int af){
	int fd, err;

	if(conn.fd >= 0){
		::close(conn.fd);

		conn.fd = -1;
	}

	fd = socket(af, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

	if(fd < 0)
		return xe_errno();
	conn.poll.set_fd(fd);
	err = conn.poll.poll(XE_POLL_OUT | XE_POLL_ONESHOT);

	if(err)
		::close(fd);
	else
		conn.fd = fd;
	return 0;
}

int xe_connection::ready(xe_connection& conn){
	uint flags;

	if(conn.timer.active())
		xe_assertz(conn.stop_timer());
	xe_return_error(conn.ready());

	conn.set_state(XE_CONNECTION_STATE_ACTIVE);
	flags = 0;

	if(!conn.send_paused){
		conn.send_paused = true;

		if(conn.readable()){
			/* if we still have data to send, poll for writable */
			flags |= XE_RESUME_SEND;
		}
	}

	if(conn.recv_paused){
		/* if the connnection was paused before fully connecting, do it now */
		conn.recv_paused = false;
		flags |= XE_PAUSE_RECV;
	}

	if(flags)
		return conn.transferctl(flags);
	return conn.ssl_enabled ? 0 : conn.poll.poll(XE_POLL_IN);
}

int xe_connection::try_connect(xe_connection& conn){
	size_t index = conn.ip_index;
	uint address_size;

	int err, family;

	union{
		sockaddr addr;
		sockaddr_in in;
		sockaddr_in6 in6;
	};

	auto& inet = conn.endpoint -> inet();
	auto& inet6 = conn.endpoint -> inet6();

	if(conn.ip_mode == XE_IP_ONLY_V6 || conn.ip_mode == XE_IP_PREFER_V6){
		family = AF_INET6;

		if(index >= inet6.size()){
			index -= inet6.size();

			if(index >= inet.size() || conn.ip_mode == XE_IP_ONLY_V6)
				return XE_EHOSTUNREACH;
			family = AF_INET;
		}
	}else{
		family = AF_INET;

		if(index >= inet.size()){
			index -= inet.size();

			if(index >= inet6.size() || conn.ip_mode == XE_IP_ONLY_V4)
				return XE_EHOSTUNREACH;
			family = AF_INET6;
		}
	}

	if(index == 0)
		xe_return_error(create_socket(conn, family));
	if(family == AF_INET){
		xe_zero(&in);
		xe_tmemcpy(&in.sin_addr, &inet[index]);

		in.sin_family = AF_INET;
		in.sin_port = conn.port;
		address_size = sizeof(in);
	}else{
		xe_zero(&in6);
		xe_tmemcpy(&in6.sin6_addr, &inet6[index]);

		in6.sin6_family = AF_INET6;
		in6.sin6_port = conn.port;
		address_size = sizeof(in6);
	}

	if(::connect(conn.fd, &addr, address_size) < 0 && (err = xe_errno()) != XE_EINPROGRESS)
		return err;
#ifdef XE_DEBUG
	conn.time = xe_time_ns();

	char ip[INET6_ADDRSTRLEN];

	inet_ntop(family, family == AF_INET ? (xe_ptr)&in.sin_addr : (xe_ptr)&in6.sin6_addr, ip, address_size);
	xe_log_debug(&conn, "connecting to %.*s:%u - trying %s", conn.host.length(), conn.host.data(), xe_ntoh(conn.port), ip);
#endif
	return 0;
}

int xe_connection::set_nodelay(bool nodelay){
	int val = nodelay ? 1 : 0;

	return setsockopt(fd, SOL_TCP, TCP_NODELAY, &val, sizeof(val)) < 0 ? xe_errno() : 0;
}

int xe_connection::set_recvbuf_size(int size){
	return setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) < 0 ? xe_errno() : 0;
}

int xe_connection::set_keepalive(bool enable, int idle){
	int val = enable ? 1 : 0;

	if(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) < 0)
		return xe_errno();
	if(enable && setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) < 0)
		return xe_errno();
	return 0;
}

int xe_connection::shutdown(uint flags){
	return ::shutdown(fd, flags) < 0 ? xe_errno() : 0;
}

int xe_connection::start_timer(ulong ms, uint flags){
	return ctx -> loop().timer_ms(timer, ms, 0, flags);
}

int xe_connection::stop_timer(){
	return ctx -> loop().cancel(timer);
}

int xe_connection::init_socket(){
	return 0;
}

void xe_connection::set_state(xe_connection_state state_){
	state = state_;
}

int xe_connection::ready(){
	return 0;
}

bool xe_connection::readable(){
	return false;
}

int xe_connection::writable(){
	return 0;
}

void xe_connection::closed(){
	ctx -> remove(*this);

	xe_log_trace(this, "closed()");
}

int xe_connection::init(xurl_ctx& ctx_){
	fd = -1;
	ctx = &ctx_.connections;
	buf = ctx_.loop().iobuf();
	poll.set_loop(ctx_.loop());

	if(buf)
		return 0;
	buf = xe_alloc_aligned<byte>(0, XE_LOOP_IOBUF_SIZE);

	return buf ? 0 : XE_ENOMEM;
}

void xe_connection::set_ssl_verify(bool verify){
	ssl_verify = verify;
}

void xe_connection::set_ip_mode(xe_ip_mode mode){
	ip_mode = mode;
}

int xe_connection::init_ssl(const xe_ssl_ctx& shared){
	int err = ssl.init(shared);

	if(!err)
		ssl_enabled = true;
	return err;
}

int xe_connection::connect(const xe_string_view& host_, ushort port_, uint timeout_ms){
	xe_string host_copy;
	int err;

	xe_log_verbose(this, "connecting to %.*s:%u", host_.length(), host_.data(), port_);

	if(ssl_enabled && ssl_verify){
		if(!host_copy.copy(host_))
			return XE_ENOMEM;
		xe_return_error(ssl.verify_host(host_copy));
	}

#ifdef XE_DEBUG
	host = host_;
#endif

	port = xe_hton(port_);
	err = ctx -> resolve(*this, host_, endpoint);

	if(err){
		if(err != XE_EINPROGRESS)
			return err;
		/* wait for name resolution */
		set_state(XE_CONNECTION_STATE_RESOLVING);
	}else{
		/* name resolution completed synchronously */
		xe_return_error(try_connect(*this));
		set_state(XE_CONNECTION_STATE_CONNECTING);

		ctx -> active++;
		ctx -> add(*this);
	}

	if(timeout_ms){
		timer.callback = timeout;

		xe_assertz(start_timer(timeout_ms));
	}

	return 0;
}

ssize_t xe_connection::send(xe_cptr data, size_t size){
	ssize_t sent;

	if(ssl_enabled)
		sent = ssl.send(data, size, XE_CONNECTION_MSG_FLAGS);
	else if((sent = ::send(fd, data, size, XE_CONNECTION_MSG_FLAGS)) < 0)
		sent = xe_errno();
	if(sent < 0)
		xe_log_trace(this, "<< connection %zi (%s)", sent, xe_strerror(sent));
	else
		xe_log_trace(this, "<< connection %zi", sent);
	return sent;
}

int xe_connection::transferctl(uint flags){
	bool prev_recv_paused = recv_paused,
		prev_send_paused = send_paused;
	if(flags & XE_PAUSE_SEND)
		send_paused = true;
	else if(flags & XE_RESUME_SEND)
		send_paused = false;
	if(flags & XE_PAUSE_RECV)
		recv_paused = true;
	else if(flags & XE_RESUME_RECV)
		recv_paused = false;
	if((send_paused == prev_send_paused && recv_paused == prev_recv_paused) ||
		state != XE_CONNECTION_STATE_ACTIVE)
		return 0;
	int err, events;

	err = 0;
	events = 0;

	if(!recv_paused)
		events |= XE_POLL_IN;
	if(!send_paused)
		events |= XE_POLL_OUT;
	err = poll.poll(events);

	if(err){
		recv_paused = prev_recv_paused;
		send_paused = prev_send_paused;
	}else if(events){
		if(prev_recv_paused && prev_send_paused) ctx -> active++;
	}else{
		ctx -> active--;
	}

	return err;
}

bool xe_connection::peer_closed(){
	if(recv(fd, null, 0, 0) < 0)
		return xe_errno() != XE_EAGAIN;
	return true;
}

void xe_connection::close(int error){
	xe_connection_state prev_state = state;

	xe_assert(state != XE_CONNECTION_STATE_CLOSED);
	set_state(XE_CONNECTION_STATE_CLOSED);
	xe_log_trace(this, "close()");

	endpoint.free();

	if(timer.active())
		xe_assertz(stop_timer());
	error = 0;

	if(prev_state != XE_CONNECTION_STATE_IDLE){
		ctx -> closing(*this);

		if(prev_state > XE_CONNECTION_STATE_RESOLVING){
			if(prev_state != XE_CONNECTION_STATE_ACTIVE || !recv_paused || !send_paused)
				ctx -> active--;
			error = poll.close();
		}
	}

	if(!error) closed();
}

xe_connection::~xe_connection(){
	if(buf != ctx -> loop().iobuf())
		xe_dealloc(buf);
	if(ssl_enabled)
		ssl.close();
	if(fd >= 0)
		::close(fd);
}

xe_cstr xe_connection::class_name(){
	return "xe_connection";
}