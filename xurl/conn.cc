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
				xe_log_debug(&conn, "connection failed, try %zu in %f ms, status: %s", conn.ip_index + 1, (xe_time_ns() - conn.time) / (float)XE_NANOS_PER_MS, xe_strerror(xe_syserror(res)));

				if(res != XE_ECONNREFUSED)
					return res;
				if(conn.ip_index < xe_max_value(conn.ip_index)){
					conn.ip_index++;

					return try_connect(conn);
				}

				return XE_ECONNREFUSED;
			}

			xe_log_verbose(&conn, "connected to %.*s:%u after %zu tries in %f ms", conn.host.length(), conn.host.data(), xe_ntohs(conn.port), (size_t)conn.ip_index + 1, (xe_time_ns() - conn.time) / (float)XE_NANOS_PER_MS);
			xe_return_error(conn.init_socket());

			if(conn.ssl_enabled){
				xe_return_error(conn.poll.poll(XE_POLL_IN));

				conn.ssl.set_fd(conn.fd);
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
				xe_log_verbose(&conn, "ssl handshake completed in %f ms", (xe_time_ns() - conn.time) / (float)XE_NANOS_PER_MS);

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

void xe_connection::start_connect(xe_endpoint& endpoint_){
	/* name resolution completed asynchronously */
	int err;

	endpoint = &endpoint_;

	if((err = try_connect(*this)))
		close(err);
	else{
		set_state(XE_CONNECTION_STATE_CONNECTING);

		ctx -> count();
		ctx -> add(*this);
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

		xe_log_trace(&conn, ">> connection %zi", result);

		if(result < 0)
			return result != XE_EAGAIN ? result : 0;
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

	conn.close(conn.state == XE_CONNECTION_STATE_HANDSHAKE ? XE_SSL : XE_ECONNREFUSED);

	return XE_ABORTED;
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
				return XE_ECONNREFUSED;
			family = AF_INET;
		}
	}else{
		family = AF_INET;

		if(index >= inet.size()){
			index -= inet.size();

			if(index >= inet6.size() || conn.ip_mode == XE_IP_ONLY_V4)
				return XE_ECONNREFUSED;
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
	}else if(family == AF_INET6){
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
	xe_log_debug(&conn, "connecting to %.*s:%u - trying %s", conn.host.length(), conn.host.data(), xe_ntohs(conn.port), ip);
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
	if(fd >= 0)
		::close(fd);
	xe_log_trace(this, "closed()");
}

int xe_connection::init(xurl_ctx& ctx_){
	fd = -1;
	ctx = &ctx_;
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
	int err;

	xe_log_verbose(this, "connecting to %.*s:%u", host_.length(), host_.data(), port_);

	if(ssl_enabled && ssl_verify)
		xe_return_error(ssl.verify_host(host_));
#ifdef XE_DEBUG
	host = host_;
#endif

	port = xe_htons(port_);
	err = ctx -> resolve(*this, host_, endpoint);

	if(err == XE_EINPROGRESS){
		/* wait for name resolution */
		set_state(XE_CONNECTION_STATE_RESOLVING);

		goto ok;
	}

	xe_return_error(err);

	/* name resolution completed synchronously */
	xe_return_error(try_connect(*this));
	set_state(XE_CONNECTION_STATE_CONNECTING);

	ctx -> count();
	ctx -> add(*this);
ok:
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
	xe_log_trace(this, "<< connection %zi", sent);

	return sent;
}

int xe_connection::transferctl(uint flags){
	bool prev_recv_paused = recv_paused,
		prev_send_paused = send_paused,
		counted = !recv_paused || !send_paused;
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
		if(!counted) ctx -> count();
	}else{
		ctx -> uncount();
	}

	return err;
}

bool xe_connection::peer_closed(){
	if(recv(fd, null, 0, 0) < 0)
		return xe_errno() != XE_EAGAIN;
	return true;
}

void xe_connection::close(int error){
	xe_assert(state != XE_CONNECTION_STATE_CLOSED);
	xe_log_trace(this, "close()");

	if(buf != ctx -> loop().iobuf())
		xe_dealloc(buf);
	if(ssl_enabled)
		ssl.close();
	if(timer.active())
		xe_assertz(stop_timer());
	if(state == XE_CONNECTION_STATE_RESOLVING)
		ctx -> resolve_remove(*this);
	else if(state > XE_CONNECTION_STATE_RESOLVING){
		if(state != XE_CONNECTION_STATE_ACTIVE || !recv_paused || !send_paused)
			ctx -> uncount();
		ctx -> remove(*this);
	}

	set_state(XE_CONNECTION_STATE_CLOSED);

	if(!poll.close()) closed();
}

xe_cstr xe_connection::class_name(){
	return "xe_connection";
}