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
	FLAG_POLL_WRITABLE = 0x4
};

static void xe_connection_close(xe_connection& conn, int error){
	conn.close();
	conn.handler.closed(error);
}

static void xe_connection_set_state(xe_connection& conn, xe_connection_state state){
	conn.state = state;
	conn.handler.state_change(state);
}

static int xe_connection_read(xe_connection& conn){
	xe_ptr buf = conn.net.get_loop().iobuf();
	ssize_t result;
	uint n = 64;

	while(n--){
		if(conn.flags & FLAG_SSL)
			result = conn.ssl.recv(buf, xe_loop::IOBUF_SIZE, 0);
		else{
			result = recv(conn.fd, buf, xe_loop::IOBUF_SIZE, 0);

			if(result < 0)
				result = xe_syserror(errno);
		}

		if(result <= 0){
			if(result == 0)
				conn.close(0);
			else if(result != XE_EAGAIN)
				return result;
			break;
		}

		result = conn.handler.write(buf, result);

		if(result <= 0){
			conn.close(result);

			return 0;
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

	int fd, err, yes;

	yes = 1;
	fd = socket(af, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

	if(fd < 0)
		return xe_syserror(errno);
	if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) < 0){
		close(fd);

		return xe_syserror(errno);
	}

	if((err = conn.net.poll(conn, EPOLL_CTL_ADD, fd, EPOLLOUT))){
		close(fd);

		return err;
	}

	conn.fd = fd;

	return 0;
}

static int xe_connection_ready(xe_connection& conn){
	int err;

	if(conn.flags & FLAG_PAUSED){
		if((err = conn.net.poll(conn, EPOLL_CTL_MOD, conn.fd, EPOLLRDHUP)))
			return err;
		conn.net.uncount();
	}

	xe_connection_set_state(conn, XE_CONNECTION_STATE_ACTIVE);

	return conn.handler.ready();
}

static int try_connect(xe_connection& conn){
	uint index = conn.endpoint_index++;
	uint address_size;

	int err;

	union{
		sockaddr addr;
		sockaddr_in in;
		sockaddr_in6 in6;
	};

	if(index >= conn.endpoint.inet.size()){
		index -= conn.endpoint.inet.size();

		if(index >= conn.endpoint.inet6.size())
			return XE_ECONNREFUSED;
		if(index == 0 && (err = create_socket(conn, AF_INET6)))
			return err;
		xe_zero(&in6);
		xe_tmemcpy(&in6.sin6_addr, &conn.endpoint.inet6[index]);

		in6.sin6_port = conn.port;
		in6.sin6_family = AF_INET6;
		address_size = sizeof(in6);
	}else{
		if(index == 0 && (err = create_socket(conn, AF_INET)))
			return err;
		xe_zero(&in);

		in.sin_addr.s_addr = conn.endpoint.inet[index].s_addr;
		in.sin_port = conn.port;
		in.sin_family = AF_INET;
		address_size = sizeof(in);
	}

	if(connect(conn.fd, &addr, address_size) < 0 && (err = errno) != EINPROGRESS)
		return xe_syserror(err);
#ifdef XE_DEBUG
	conn.time = xe_time_ns();

	char ip[INET6_ADDRSTRLEN];

	if(address_size == sizeof(in))
		inet_ntop(AF_INET, &in.sin_addr, ip, address_size);
	else
		inet_ntop(AF_INET6, &in6.sin6_addr, ip, address_size);
	xe_log_trace("xe_connection", &conn, "connecting to %.*s:%d - trying %s", conn.host.length(), conn.host.c_str(), htons(conn.port), ip);
#endif
	return 0;
}

static int xe_connection_connected(xe_connection& conn){
	int err;

	xe_log_debug("xe_connection", &conn, "connected after %d tries in %f ms", conn.endpoint_index, (xe_time_ns() - conn.time) / 1e6);

	if((err = conn.net.poll(conn, EPOLL_CTL_MOD, conn.fd, EPOLLIN)))
		return err;
	if(conn.flags & FLAG_SSL){
		if((err = conn.ssl.verify_host(conn.host)))
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
				return xe_syserror(errno);
			if(res){
				xe_log_trace("xe_connection", &conn, "connected failed, try %d in %f ms, status: %s", conn.endpoint_index, (xe_time_ns() - conn.time) / 1e6, strerror(res));

				return try_connect(conn);
			}

			if((res = xe_connection_connected(conn)))
				return res;
			if(!(conn.flags & FLAG_SSL))
				break;
		case XE_CONNECTION_STATE_HANDSHAKE:
			if(res & EPOLLRDHUP)
				return XE_ECONNRESET;
			res = conn.ssl.connect(MSG_NOSIGNAL);

			if(res == 0){
				xe_log_debug("xe_connection", &conn, "ssl handshake completed in %f ms", (xe_time_ns() - conn.time) / 1e6);

				return xe_connection_ready(conn);
			}else if(res != XE_EAGAIN){
				return res;
			}

			break;
		case XE_CONNECTION_STATE_ACTIVE:
			if(res & EPOLLRDHUP)
				return XE_ECONNRESET;
			if(res & EPOLLOUT && (res = conn.handler.writable()))
				return res;
			return xe_connection_read(conn);

			break;
		default:
			xe_notreached;
	}

	return 0;
}

xe_connection::xe_connection(xe_net_ctx& net, xe_connection_handler& data): net(net), handler(data){
	fd = -1;
}

int xe_connection::init_ssl(xe_ssl_ctx& shared){
	int result = ssl.init(shared);

	if(!result)
		flags |= FLAG_SSL;
	return result;
}

int xe_connection::connect(xe_string _host, int _port){
	xe_log_debug("xe_connection", this, "connecting to %.*s:%d", _host.length(), _host.c_str(), _port);

	int err = net.resolve(*this, _host, endpoint);

	host = _host;
	port = htons(_port);

	if(err == XE_EINPROGRESS){
		xe_connection_set_state(*this, XE_CONNECTION_STATE_RESOLVING);

		return 0;
	}

	if(err)
		return err;
	err = try_connect(*this);

	if(!err){
		net.count();
		net.add(*this);

		xe_connection_set_state(*this, XE_CONNECTION_STATE_CONNECTING);
	}

	return err;
}

ssize_t xe_connection::send(xe_cptr data, size_t size){
	ssize_t sent;

	if(flags & FLAG_SSL)
		return ssl.send(data, size, MSG_NOSIGNAL);
	if((sent = ::send(fd, data, size, MSG_NOSIGNAL)) < 0)
		return xe_syserror(errno);
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

		return net.poll(*this, EPOLL_CTL_MOD, fd, EPOLLIN | EPOLLOUT);
	}else{
		if(!(flags & FLAG_POLL_WRITABLE))
			return 0;
		flags &= ~FLAG_POLL_WRITABLE;

		return net.poll(*this, EPOLL_CTL_MOD, fd, EPOLLIN);
	}
}

int xe_connection::pause(bool paused){
	int err = 0;

	if(!paused){
		if(!(flags & FLAG_PAUSED))
			return 0;
		flags &= ~FLAG_PAUSED;

		if(state == XE_CONNECTION_STATE_ACTIVE){
			if(flags & FLAG_POLL_WRITABLE)
				err = poll_writable(true);
			else
				err = net.poll(*this, EPOLL_CTL_MOD, fd, EPOLLIN);
			if(!err)
				net.count();
		}
	}else{
		if(flags & FLAG_PAUSED)
			return 0;
		flags |= FLAG_PAUSED;

		if(state == XE_CONNECTION_STATE_ACTIVE){
			err = net.poll(*this, EPOLL_CTL_MOD, fd, EPOLLRDHUP);

			if(!err)
				net.uncount();
		}
	}

	return err;
}

void xe_connection::close(int error){
	close();

	handler.closed(error);
}

void xe_connection::close(){
	if(flags & FLAG_SSL)
		ssl.close();
	if(fd != -1)
		::close(fd);
	if(state > XE_CONNECTION_STATE_RESOLVING){
		if(state != XE_CONNECTION_STATE_ACTIVE || !(flags & FLAG_PAUSED))
			net.uncount();
		net.remove(*this);
	}

	xe_connection_set_state(*this, XE_CONNECTION_STATE_CLOSED);

	xe_log_trace("xe_connection", this, "close()");
}

xe_connection* xe_connection::alloc(xe_net_ctx& net, xe_connection_handler& data){
	return xe_znew<xe_connection>(net, data);
}

void xe_connection::io(int res){
	res = xe_connection_io(*this, res);

	if(res)
		close(res);
}

void xe_connection::resolved(xe_endpoint& _endpoint, int status){
	if(!status){
		endpoint = _endpoint;
		status = try_connect(*this);
	}

	if(status)
		close(status);
	else{
		xe_connection_set_state(*this, XE_CONNECTION_STATE_CONNECTING);

		net.count();
		net.add(*this);
	}
}