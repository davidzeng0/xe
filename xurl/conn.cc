#include <unistd.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "xutil/clock.h"
#include "conn.h"
#include "xutil/inet.h"

using namespace xurl;

int xe_connection::io(xe_connection& conn, int res){
	socklen_t len = sizeof(res);

	switch(conn.state){
		case XE_CONNECTION_STATE_CONNECTING:
			if(getsockopt(conn.fd, SOL_SOCKET, SO_ERROR, &res, &len) < 0)
				return xe_errno();
			if(res){
				xe_log_debug(&conn, "connection failed, try %u in %f ms, status: %s", conn.endpoint_index, (xe_time_ns() - conn.time) / (float)XE_NANOS_PER_MS, xe_strerror(xe_syserror(res)));

				return try_connect(conn);
			}

			xe_log_verbose(&conn, "connected to %.*s:%u after %u tries in %f ms", conn.host.length(), conn.host.c_str(), xe_htons(conn.port), conn.endpoint_index, (xe_time_ns() - conn.time) / (float)XE_NANOS_PER_MS);

			if(conn.ssl_enabled){
				xe_return_error(conn.ctx -> poll(conn, EPOLL_CTL_MOD, conn.fd, EPOLLIN));

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
			res = conn.ssl.connect(MSG_NOSIGNAL);

			if(res == 0){
				xe_log_verbose(&conn, "ssl handshake completed in %f ms", (xe_time_ns() - conn.time) / (float)XE_NANOS_PER_MS);

				return ready(conn);
			}else if(res != XE_EAGAIN){
				return res;
			}

			break;
		case XE_CONNECTION_STATE_ACTIVE:
			if(res & EPOLLRDHUP){
				conn.close(0);

				break;
			}

			if(res & EPOLLOUT){
				/* send data */
				xe_return_error(conn.writable());

				if(!conn.readable())
					xe_return_error(conn.transferctl(XE_PAUSE_SEND));
			}

			return (res & EPOLLIN) ? socket_read(conn) : 0;
		default:
			xe_notreached();

			break;
	}

	return 0;
}

void xe_connection::io(int res){
	if((res = io(*this, res)))
		close(res);
}

void xe_connection::start_connect(xe_endpoint& endpoint_, int status){
	/* name resolution completed asynchronously */
	if(!status){
		endpoint = &endpoint_;
		status = try_connect(*this);
	}

	if(status)
		close(status);
	else{
		set_state(XE_CONNECTION_STATE_CONNECTING);

		ctx -> count(*this);
		ctx -> add(*this);
	}
}

int xe_connection::socket_read(xe_connection& conn){
	xe_ptr buf = conn.buf;
	ssize_t result;
	uint n = 64;

	while(n--){
		if(conn.ssl_enabled)
			result = conn.ssl.recv(buf, XE_LOOP_IOBUF_SIZE, 0);
		else{
			result = recv(conn.fd, buf, XE_LOOP_IOBUF_SIZE, 0);

			if(result < 0)
				result = xe_errno();
		}

		xe_log_trace(&conn, "recv: %zi", result);

		if(result < 0){
			if(result != XE_EAGAIN)
				return result;
			break;
		}

		result = conn.data(buf, result);

		if(result <= 0){
			conn.close(result);

			break;
		}

		if(conn.recv_paused)
			break;
	}

	return 0;
}

int xe_connection::create_socket(xe_connection& conn, int af){
	if(conn.fd != -1){
		::close(conn.fd);

		conn.fd = -1;
	}

	int fd, err, yes, idle;

	yes = 1;
	idle = 60;
	fd = socket(af, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

	if(fd < 0)
		return xe_errno();
	if((err = conn.ctx -> poll(conn, EPOLL_CTL_ADD, fd, EPOLLOUT))){
		::close(fd);

		return err;
	}

	do{
		if(setsockopt(fd, SOL_TCP, TCP_NODELAY, &yes, sizeof(yes)) < 0)
			break;
		if(conn.recvbuf_size && setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &conn.recvbuf_size, sizeof(conn.recvbuf_size)) < 0)
			break;
		if(conn.tcp_keepalive){
			if(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes)) < 0)
				break;
			if(setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) < 0)
				break;
		}

		conn.fd = fd;

		return 0;
	}while(false);

	::close(fd);

	return xe_errno();
}

int xe_connection::ready(xe_connection& conn){
	int err, flags = 0;

	xe_return_error(conn.ready());

	conn.set_state(XE_CONNECTION_STATE_ACTIVE);

	if(!conn.send_paused && conn.readable()){
		/* if we still have data to send, poll for writable */
		flags |= XE_RESUME_SEND;
	}

	conn.send_paused = true;

	if(conn.recv_paused){
		/* if the connnection was paused before fully connecting, do it now */
		conn.recv_paused = false;
		flags |= XE_PAUSE_RECV;
	}

	if(flags)
		return conn.transferctl(flags);
	return conn.ssl_enabled ? 0 : conn.ctx -> poll(conn, EPOLL_CTL_MOD, conn.fd, EPOLLIN);
}

int xe_connection::try_connect(xe_connection& conn){
	uint index = conn.endpoint_index++;
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

		in.sin_addr.s_addr = inet[index].s_addr;
		in.sin_port = conn.port;
		in.sin_family = AF_INET;
		address_size = sizeof(in);
	}else if(family == AF_INET6){
		xe_zero(&in6);
		xe_tmemcpy(&in6.sin6_addr, &inet6[index]);

		in6.sin6_port = conn.port;
		in6.sin6_family = AF_INET6;
		address_size = sizeof(in6);
	}

	if(::connect(conn.fd, &addr, address_size) < 0 && (err = xe_errno()) != XE_EINPROGRESS)
		return err;
#ifdef XE_DEBUG
	conn.time = xe_time_ns();

	char ip[INET6_ADDRSTRLEN];

	if(family == AF_INET)
		inet_ntop(AF_INET, &in.sin_addr, ip, address_size);
	else
		inet_ntop(AF_INET6, &in6.sin6_addr, ip, address_size);
	xe_log_debug(&conn, "connecting to %.*s:%u - trying %s", conn.host.length(), conn.host.c_str(), xe_htons(conn.port), ip);
#endif
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

int xe_connection::init(xurl_ctx& ctx_){
	fd = -1;
	ctx = &ctx_;
	buf = ctx_.loop().iobuf();

	if(buf)
		return 0;
	buf = xe_alloc<byte>(XE_LOOP_IOBUF_SIZE);

	return buf ? 0 : XE_ENOMEM;
}

void xe_connection::set_ssl_verify(bool verify){
	ssl_verify = verify;
}

void xe_connection::set_ip_mode(xe_ip_mode mode){
	ip_mode = mode;
}

void xe_connection::set_recvbuf_size(uint size){
	recvbuf_size = size;
}

void xe_connection::set_tcp_keepalive(bool keepalive){
	tcp_keepalive = keepalive;
}

void xe_connection::set_connect_timeout(uint timeout_ms){

}

int xe_connection::init_ssl(xe_ssl_ctx& shared){
	int err = ssl.init(shared);

	if(!err)
		ssl_enabled = true;
	return err;
}

int xe_connection::connect(const xe_string_view& host_, int port_){
	int err;

	xe_log_verbose(this, "connecting to %.*s:%u", host_.length(), host_.c_str(), port_);

	if(ssl_enabled && ssl_verify)
		xe_return_error(ssl.verify_host(host_));
	port = xe_htons(port_);
	err = ctx -> resolve(*this, host_, endpoint);
#ifdef XE_DEBUG
	host = host_;
#endif
	if(err == XE_EINPROGRESS){
		/* wait for name resolution */
		set_state(XE_CONNECTION_STATE_RESOLVING);

		return 0;
	}

	xe_return_error(err);
	/* name resolution completed synchronously */
	err = try_connect(*this);

	if(!err){
		set_state(XE_CONNECTION_STATE_CONNECTING);

		ctx -> count(*this);
		ctx -> add(*this);
	}

	return err;
}

ssize_t xe_connection::send(xe_cptr data, size_t size){
	ssize_t sent;

	if(ssl_enabled)
		return ssl.send(data, size, MSG_NOSIGNAL);
	if((sent = ::send(fd, data, size, MSG_NOSIGNAL)) < 0)
		return xe_errno();
	return sent;
}

int xe_connection::transferctl(uint flags){
	bool prev_recv_paused = recv_paused,
		prev_send_paused = send_paused,
		changed = false,
		counted = !recv_paused || !send_paused;
	int err = 0;

	if(flags & XE_PAUSE_SEND){
		if(!send_paused){
			send_paused = true;
			changed = true;
		}
	}else if(flags & XE_RESUME_SEND){
		if(send_paused){
			send_paused = false;
			changed = true;
		}
	}

	if(flags & XE_PAUSE_RECV){
		if(!recv_paused){
			recv_paused = true;
			changed = true;
		}
	}else if(flags & XE_RESUME_RECV){
		if(recv_paused){
			recv_paused = false;
			changed = true;
		}
	}

	if(!changed)
		return 0;
	if(state == XE_CONNECTION_STATE_ACTIVE){
		flags = 0;

		if(!recv_paused)
			flags |= EPOLLIN;
		if(!send_paused)
			flags |= EPOLLOUT;
		if(flags){
			err = ctx -> poll(*this, counted ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, fd, flags);

			if(err){
				recv_paused = false;
				send_paused = false;
			}else if(!counted){
				ctx -> count(*this);
			}
		}else{
			err = ctx -> poll(*this, EPOLL_CTL_DEL, fd, 0);

			if(!err)
				ctx -> uncount(*this);
			else{
				recv_paused = prev_recv_paused;
				send_paused = prev_send_paused;
			}
		}
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

	if(ssl_enabled)
		ssl.close();
	if(fd != -1)
		::close(fd);
	if(state > XE_CONNECTION_STATE_RESOLVING){
		if(state != XE_CONNECTION_STATE_ACTIVE || !recv_paused || !send_paused)
			ctx -> uncount(*this);
		ctx -> remove(*this);
	}

	set_state(XE_CONNECTION_STATE_CLOSED);
	xe_log_trace(this, "close()");
}

xe_connection::~xe_connection(){
	if(buf != ctx -> loop().iobuf())
		xe_dealloc(buf);
}

xe_cstr xe_connection::class_name(){
	return "xe_connection";
}