#include <unistd.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "conn.h"

using namespace xe_net;

enum xe_connection_flags{
	FLAG_NONE = 0x0,
	FLAG_SSL = 0x1,
	FLAG_PAUSED = 0x2,
	FLAG_POLL_WRITABLE = 0x4,
	FLAG_SSL_VERIFY = 0x8,
	FLAG_KEEPALIVE = 0x10
};

static void xe_connection_close(xe_connection& conn, int error){
	conn.close();
	conn.handler -> closed(error);
}

static void xe_connection_set_state(xe_connection& conn, xe_connection_state state){
	conn.state = state;
	conn.handler -> state_change(state);
}

static int xe_connection_read(xe_connection& conn){
	xe_ptr buf = conn.net -> get_loop().iobuf();
	ssize_t result;
	uint n = 64;

	while(n--){
		if(conn.flags & FLAG_SSL)
			result = conn.ssl.recv(buf, xe_loop::IOBUF_SIZE, 0);
		else{
			result = recv(conn.fd, buf, xe_loop::IOBUF_SIZE, 0);

			if(result < 0)
				result = xe_errno();
		}

		xe_log_trace("xe_connection", &conn, "recv: %zi", result);

		if(result < 0){
			if(result != XE_EAGAIN)
				return result;
			break;
		}

		result = conn.handler -> write(buf, result);

		if(result <= 0){
			conn.close(result);

			break;
		}

		if(conn.flags & FLAG_PAUSED)
			break;
	}

	return 0;
}

static int create_socket(xe_connection& conn, int af){
	if(conn.fd != -1){
		close(conn.fd);

		conn.fd = -1;
	}

	int fd, err, yes, idle;

	yes = 1;
	idle = 60;
	fd = socket(af, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

	if(fd < 0)
		return xe_errno();
	if(setsockopt(fd, SOL_TCP, TCP_NODELAY, &yes, sizeof(yes)) < 0)
		goto syserr;
	if(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes)) < 0)
		goto syserr;
	if(setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) < 0)
		goto syserr;
	if(conn.recvbuf_size && setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &conn.recvbuf_size, sizeof(conn.recvbuf_size)) < 0)
		goto syserr;
	if((err = conn.net -> poll(conn, EPOLL_CTL_ADD, fd, EPOLLOUT))){
		close(fd);

		return err;
	}

	conn.fd = fd;

	return 0;
syserr:
	close(fd);

	return xe_errno();
}

static int xe_connection_ready(xe_connection& conn){
	int err;

	xe_connection_set_state(conn, XE_CONNECTION_STATE_ACTIVE);

	if(conn.flags & FLAG_PAUSED){
		conn.flags &= ~FLAG_PAUSED;

		/* if the connnection was paused or put on keepalive before fully connecting, do it now */
		if(conn.flags & FLAG_KEEPALIVE)
			err = conn.keepalive();
		else
			err = conn.pause(true);
		if(err)
			return err;
	}

	return conn.handler -> ready();
}

static int try_connect(xe_connection& conn){
	uint index = conn.endpoint_index++;
	uint address_size;

	int err, family;

	union{
		sockaddr addr;
		sockaddr_in in;
		sockaddr_in6 in6;
	};

	if(conn.ip_mode == XE_IP_ONLY_V6 || conn.ip_mode == XE_IP_PREFER_V6){
		family = AF_INET6;

		if(index >= conn.endpoint.inet6.size()){
			index -= conn.endpoint.inet6.size();

			if(index >= conn.endpoint.inet.size() || conn.ip_mode == XE_IP_ONLY_V6)
				return XE_ECONNREFUSED;
			family = AF_INET;
		}
	}else{
		family = AF_INET;

		if(index >= conn.endpoint.inet.size()){
			index -= conn.endpoint.inet.size();

			if(index >= conn.endpoint.inet6.size() || conn.ip_mode == XE_IP_ONLY_V4)
				return XE_ECONNREFUSED;
			family = AF_INET6;
		}
	}

	if(family == AF_INET){
		if(index == 0 && (err = create_socket(conn, AF_INET)))
			return err;
		xe_zero(&in);

		in.sin_addr.s_addr = conn.endpoint.inet[index].s_addr;
		in.sin_port = conn.port;
		in.sin_family = AF_INET;
		address_size = sizeof(in);
	}else if(family == AF_INET6){
		if(index == 0 && (err = create_socket(conn, AF_INET6)))
			return err;
		xe_zero(&in6);
		xe_tmemcpy(&in6.sin6_addr, &conn.endpoint.inet6[index]);

		in6.sin6_port = conn.port;
		in6.sin6_family = AF_INET6;
		address_size = sizeof(in6);
	}

	if(connect(conn.fd, &addr, address_size) < 0 && (err = xe_errno()) != XE_EINPROGRESS)
		return err;
#ifdef XE_DEBUG
	conn.time = xe_time_ns();

	char ip[INET6_ADDRSTRLEN];

	if(address_size == sizeof(in))
		inet_ntop(AF_INET, &in.sin_addr, ip, address_size);
	else
		inet_ntop(AF_INET6, &in6.sin6_addr, ip, address_size);
	xe_log_debug("xe_connection", &conn, "connecting to %.*s:%u - trying %s", conn.host.length(), conn.host.c_str(), htons(conn.port), ip);
#endif
	return 0;
}

static int xe_connection_connected(xe_connection& conn){
	int err;

	xe_log_verbose("xe_connection", &conn, "connected to %.*s:%u after %u tries in %f ms", conn.host.length(), conn.host.c_str(), htons(conn.port), conn.endpoint_index, (xe_time_ns() - conn.time) / 1e6);

	if((err = conn.net -> poll(conn, EPOLL_CTL_MOD, conn.fd, EPOLLIN)))
		return err;
	if(conn.flags & FLAG_SSL){
		if(conn.flags & FLAG_SSL_VERIFY && (err = conn.ssl.verify_host(conn.host)))
			return err;
		conn.ssl.set_fd(conn.fd);

		xe_connection_set_state(conn, XE_CONNECTION_STATE_HANDSHAKE);

#ifdef XE_DEBUG
		conn.time = xe_time_ns();
#endif
	}else{
		return xe_connection_ready(conn);
	}

	return 0;
}

static int xe_connection_io(xe_connection& conn, int res){
	socklen_t len = sizeof(res);

	switch(conn.state){
		case XE_CONNECTION_STATE_CONNECTING:
			if(getsockopt(conn.fd, SOL_SOCKET, SO_ERROR, &res, &len) < 0)
				return xe_errno();
			if(res){
				xe_log_debug("xe_connection", &conn, "connected failed, try %u in %f ms, status: %s", conn.endpoint_index, (xe_time_ns() - conn.time) / 1e6, xe_strerror(xe_syserror(res)));

				return try_connect(conn);
			}

			if((res = xe_connection_connected(conn)))
				return res;
			if(!(conn.flags & FLAG_SSL))
				break;
			/* fallthrough: send ssl hello */
		case XE_CONNECTION_STATE_HANDSHAKE:
			res = conn.ssl.connect(MSG_NOSIGNAL);

			if(res == 0){
				xe_log_verbose("xe_connection", &conn, "ssl handshake completed in %f ms", (xe_time_ns() - conn.time) / 1e6);

				return xe_connection_ready(conn);
			}else if(res != XE_EAGAIN){
				return res;
			}

			break;
		case XE_CONNECTION_STATE_ACTIVE:
			if(res & EPOLLRDHUP){
				conn.close(0);

				break;
			}

			/* send data */
			if(res & EPOLLOUT && (res = conn.handler -> writable()))
				return res;
			return xe_connection_read(conn);
		default:
			xe_notreached();
	}

	return 0;
}

void xe_connection::init(xe_net_ctx& net_, xe_connection_handler& handler_){
	net = &net_;
	handler = &handler_;
	fd = -1;
}

int xe_connection::init_ssl(xe_ssl_ctx& shared){
	int result = ssl.init(shared);

	if(!result)
		flags |= FLAG_SSL;
	return result;
}

void xe_connection::set_ssl_verify(bool verify){
	if(verify)
		flags |= FLAG_SSL_VERIFY;
	else
		flags &= ~FLAG_SSL_VERIFY;
}

void xe_connection::set_ip_mode(xe_ip_mode mode){
	ip_mode = mode;
}

void xe_connection::set_recvbuf_size(uint size){
	recvbuf_size = size;
}

void xe_connection::set_connect_timeout(uint timeout_ms){

}

int xe_connection::connect(xe_string host_, int port_){
	xe_log_verbose("xe_connection", this, "connecting to %.*s:%u", host_.length(), host_.c_str(), port_);

	int err = net -> resolve(*this, host_, endpoint);

	host = host_;
	port = htons(port_);

	if(err == XE_EINPROGRESS){
		/* wait for name resolution */
		xe_connection_set_state(*this, XE_CONNECTION_STATE_RESOLVING);

		return 0;
	}

	if(err)
		return err;
	/* name resolution completed synchronously */
	err = try_connect(*this);

	if(!err){
		net -> count();
		net -> add(*this);

		xe_connection_set_state(*this, XE_CONNECTION_STATE_CONNECTING);
	}

	return err;
}

ssize_t xe_connection::send(xe_cptr data, size_t size){
	ssize_t sent;

	if(flags & FLAG_SSL)
		return ssl.send(data, size, MSG_NOSIGNAL);
	if((sent = ::send(fd, data, size, MSG_NOSIGNAL)) < 0)
		return xe_errno();
	return sent;
}

int xe_connection::poll_writable(bool poll){
	if(flags & FLAG_PAUSED){
		if(poll)
			flags |= FLAG_POLL_WRITABLE;
		else
			flags &= ~FLAG_POLL_WRITABLE;
		return 0;
	}

	if(poll){
		if(flags & FLAG_POLL_WRITABLE)
			return 0;
		flags |= FLAG_POLL_WRITABLE;

		return net -> poll(*this, EPOLL_CTL_MOD, fd, EPOLLIN | EPOLLOUT);
	}else{
		if(!(flags & FLAG_POLL_WRITABLE))
			return 0;
		flags &= ~FLAG_POLL_WRITABLE;

		return net -> poll(*this, EPOLL_CTL_MOD, fd, EPOLLIN);
	}
}

int xe_connection::pause(bool paused){
	int err = 0;

	if((flags & FLAG_PAUSED) != 0 == paused)
		return 0;
	if(!paused){
		int keepalive = flags & FLAG_KEEPALIVE;

		flags &= ~(FLAG_PAUSED | FLAG_KEEPALIVE);

		if(state == XE_CONNECTION_STATE_ACTIVE){
			err = net -> poll(*this, keepalive ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, fd, EPOLLIN | (flags & FLAG_POLL_WRITABLE ? EPOLLOUT : 0));

			if(!err)
				net -> count();
			else
				flags |= FLAG_PAUSED | keepalive;
		}
	}else{
		flags |= FLAG_PAUSED;

		if(state == XE_CONNECTION_STATE_ACTIVE){
			err = net -> poll(*this, EPOLL_CTL_DEL, fd, 0);

			if(!err)
				net -> uncount();
			else
				flags &= ~FLAG_PAUSED;
		}
	}

	return err;
}

int xe_connection::keepalive(){
	int err = 0, paused;

	if(flags & FLAG_PAUSED && flags & FLAG_KEEPALIVE)
		return 0;
	paused = flags & FLAG_PAUSED;
	flags |= FLAG_PAUSED | FLAG_KEEPALIVE;

	if(state == XE_CONNECTION_STATE_ACTIVE){
		err = net -> poll(*this, paused ? EPOLL_CTL_ADD : EPOLL_CTL_MOD, fd, EPOLLRDHUP);

		if(!err){
			if(!paused)
				net -> uncount();
		}else{
			flags &= ~FLAG_KEEPALIVE;

			if(!paused)
				flags &= ~FLAG_PAUSED;
		}
	}

	return err;
}

void xe_connection::close(int error){
	close();

	handler -> closed(error);
}

void xe_connection::close(){
	if(flags & FLAG_SSL)
		ssl.close();
	if(fd != -1)
		::close(fd);
	if(state > XE_CONNECTION_STATE_RESOLVING){
		if(state != XE_CONNECTION_STATE_ACTIVE || !(flags & FLAG_PAUSED))
			net -> uncount();
		net -> remove(*this);
	}

	xe_connection_set_state(*this, XE_CONNECTION_STATE_CLOSED);

	xe_log_trace("xe_connection", this, "close()");
}

void xe_connection::io(int res){
	res = xe_connection_io(*this, res);

	if(res)
		close(res);
}

void xe_connection::resolved(xe_endpoint& endpoint_, int status){
	/* name resolution completed asynchronously */
	if(!status){
		endpoint = endpoint_;
		status = try_connect(*this);
	}

	if(status)
		close(status);
	else{
		xe_connection_set_state(*this, XE_CONNECTION_STATE_CONNECTING);

		net -> count();
		net -> add(*this);
	}
}