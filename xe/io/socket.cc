#include <unistd.h>
#include "socket.h"
#include "../error.h"

enum xe_socket_state{
	XE_SOCKET_NONE = 0,
	XE_SOCKET_OPENING,
	XE_SOCKET_READY,
	XE_SOCKET_LISTENING,
	XE_SOCKET_CONNECTING,
	XE_SOCKET_CONNECTED
};

void xe_socket_req::complete(xe_req& req, int res, uint flags){
	xe_socket_req& sock_req = (xe_socket_req&)req;

	sock_req.socket -> open(res);

	if(res > 0)
		res = 0;
	if(sock_req.callback) sock_req.callback(sock_req, res);
}

void xe_socket_promise::complete(xe_req& req, int res, uint flags){
	xe_socket_promise& promise = (xe_socket_promise&)req;

	promise.socket -> open(res);

	if(res > 0)
		res = 0;
	xe_promise::complete(req, res, flags);
}

xe_socket_promise::xe_socket_promise(){
	event = complete;
}

void xe_connect_req::complete(xe_req& req, int res, uint flags){
	xe_connect_req& conn_req = (xe_connect_req&)req;

	conn_req.socket -> connect(res);

	if(res > 0)
		res = 0;
	if(conn_req.callback) conn_req.callback(conn_req, res);
}

void xe_connect_promise::complete(xe_req& req, int res, uint flags){
	xe_connect_promise& promise = (xe_connect_promise&)req;

	promise.socket -> connect(res);

	if(res > 0)
		res = 0;
	xe_promise::complete(req, res, flags);
}

xe_connect_promise::xe_connect_promise(){
	event = complete;
}

void xe_socket::open(int res){
	if(res >= 0){
		fd_ = res;
		state = XE_SOCKET_READY;
	}else{
		state = XE_SOCKET_NONE;
	}
}

void xe_socket::connect(int res){
	state = res ? XE_SOCKET_READY : XE_SOCKET_CONNECTED;
}

int xe_socket::init_sync(int af, int type, int proto){
	if(state != XE_SOCKET_NONE)
		return XE_STATE;
	int fd = ::socket(af, type, proto);

	if(fd < 0)
		return xe_errno();
	init_fd(fd);

	return 0;
}

int xe_socket::init_fd(int fd){
	if(state != XE_SOCKET_NONE)
		return XE_STATE;
	fd_ = fd;
	state = XE_SOCKET_READY;

	return 0;
}

int xe_socket::accept(int fd){
	if(state != XE_SOCKET_NONE)
		return XE_STATE;
	fd_ = fd;
	state = XE_SOCKET_CONNECTED;

	return 0;
}

int xe_socket::init(xe_socket_req& req, int af, int type, int proto){
	if(state == XE_SOCKET_OPENING)
		return XE_EALREADY;
	if(state != XE_SOCKET_NONE)
		return XE_STATE;
	xe_return_error(loop_ -> run(req, xe_op::socket(af, type, proto, 0)));

	req.socket = this;
	state = XE_SOCKET_OPENING;

	return 0;
}

xe_socket_promise xe_socket::init(int af, int type, int proto){
	xe_socket_promise promise;
	int res;

	if(state == XE_SOCKET_OPENING)
		res = XE_EALREADY;
	else if(state != XE_SOCKET_NONE)
		res = XE_STATE;
	else
		res = loop_ -> run(promise, xe_op::socket(af, type, proto, 0));
	if(res){
		promise.result_ = res;
		promise.ready_ = true;
	}else{
		promise.socket = this;
		state = XE_SOCKET_OPENING;
	}

	return promise;
}

int xe_socket::accept_sync(sockaddr* addr, socklen_t* addrlen, uint flags){
	int fd = accept4(fd_, addr, addrlen, flags);

	return fd < 0 ? xe_errno() : fd;
}

int xe_socket::connect_sync(const sockaddr* addr, socklen_t addrlen){
	if(state != XE_SOCKET_READY && state != XE_SOCKET_CONNECTED)
		return XE_STATE;
	if(::connect(fd_, addr, addrlen) < 0)
		return xe_errno();
	state = XE_SOCKET_CONNECTED;

	return 0;
}

int xe_socket::accept(xe_req& req, sockaddr* addr, socklen_t* addrlen, uint flags){
	if(state != XE_SOCKET_LISTENING)
		return XE_STATE;
	return loop_ -> run(req, xe_op::accept(fd_, addr, addrlen, flags));
}

int xe_socket::connect(xe_connect_req& req, const sockaddr* addr, socklen_t addrlen){
	if(state == XE_SOCKET_CONNECTING)
		return XE_EALREADY;
	if(state != XE_SOCKET_READY)
		return XE_STATE;
	xe_return_error(loop_ -> run(req, xe_op::connect(fd_, addr, addrlen)));

	req.socket = this;
	state = XE_SOCKET_CONNECTING;

	return 0;
}

xe_promise xe_socket::accept(sockaddr* addr, socklen_t* addrlen, uint flags){
	if(state != XE_SOCKET_LISTENING)
		return xe_promise::done(XE_STATE);
	return loop_ -> run(xe_op::accept(fd_, addr, addrlen, flags));
}

xe_connect_promise xe_socket::connect(const sockaddr* addr, socklen_t addrlen){
	xe_connect_promise promise;
	int res;

	if(state == XE_SOCKET_CONNECTING)
		res = XE_EALREADY;
	else if(state != XE_SOCKET_READY)
		res = XE_STATE;
	else
		res = loop_ -> run(promise, xe_op::connect(fd_, addr, addrlen));
	if(res){
		promise.result_ = res;
		promise.ready_ = true;
	}else{
		promise.socket = this;
		state = XE_SOCKET_CONNECTING;
	}

	return promise;
}

int xe_socket::recv_sync(xe_ptr buf, uint len, uint flags){
	if(state != XE_SOCKET_CONNECTED)
		return XE_ENOTCONN;
	int recvd = ::recv(fd_, buf, len, flags);

	return recvd < 0 ? xe_errno() : recvd;
}

int xe_socket::send_sync(xe_cptr buf, uint len, uint flags){
	if(state != XE_SOCKET_CONNECTED)
		return XE_ENOTCONN;
	int sent = ::send(fd_, buf, len, flags);

	return sent < 0 ? xe_errno() : sent;
}

int xe_socket::recv(xe_req& req, xe_ptr buf, uint len, uint flags){
	if(state != XE_SOCKET_CONNECTED)
		return XE_ENOTCONN;
	return loop_ -> run(req, xe_op::recv(fd_, buf, len, flags));
}

int xe_socket::send(xe_req& req, xe_cptr buf, uint len, uint flags){
	if(state != XE_SOCKET_CONNECTED)
		return XE_ENOTCONN;
	return loop_ -> run(req, xe_op::send(fd_, buf, len, flags));
}

xe_promise xe_socket::recv(xe_ptr buf, uint len, uint flags){
	if(state != XE_SOCKET_CONNECTED)
		return xe_promise::done(XE_ENOTCONN);
	return loop_ -> run(xe_op::recv(fd_, buf, len, flags));
}

xe_promise xe_socket::send(xe_cptr buf, uint len, uint flags){
	if(state != XE_SOCKET_CONNECTED)
		return xe_promise::done(XE_ENOTCONN);
	return loop_ -> run(xe_op::send(fd_, buf, len, flags));
}

int xe_socket::recvmsg_sync(msghdr* msg, uint flags){
	if(state != XE_SOCKET_CONNECTED)
		return XE_ENOTCONN;
	int recvd = ::recvmsg(fd_, msg, flags);

	return recvd < 0 ? xe_errno() : recvd;
}

int xe_socket::sendmsg_sync(const msghdr* msg, uint flags){
	if(state != XE_SOCKET_CONNECTED)
		return XE_ENOTCONN;
	int sent = ::sendmsg(fd_, msg, flags);

	return sent < 0 ? xe_errno() : sent;
}

int xe_socket::recvmsg(xe_req& req, msghdr* msg, uint flags){
	if(state != XE_SOCKET_CONNECTED)
		return XE_ENOTCONN;
	return loop_ -> run(req, xe_op::recvmsg(fd_, msg, flags));
}

int xe_socket::sendmsg(xe_req& req, const msghdr* msg, uint flags){
	if(state != XE_SOCKET_CONNECTED)
		return XE_ENOTCONN;
	return loop_ -> run(req, xe_op::sendmsg(fd_, msg, flags));
}

xe_promise xe_socket::recvmsg(msghdr* msg, uint flags){
	if(state != XE_SOCKET_CONNECTED)
		return xe_promise::done(XE_ENOTCONN);
	return loop_ -> run(xe_op::recvmsg(fd_, msg, flags));
}

xe_promise xe_socket::sendmsg(const msghdr* msg, uint flags){
	if(state != XE_SOCKET_CONNECTED)
		return xe_promise::done(XE_ENOTCONN);
	return loop_ -> run(xe_op::sendmsg(fd_, msg, flags));
}

int xe_socket::shutdown_sync(int how){
	if(state != XE_SOCKET_CONNECTED)
		return XE_ENOTCONN;
	return ::shutdown(fd_, how) < 0 ? xe_errno() : 0;
}

int xe_socket::shutdown(xe_req& req, int how){
	if(state != XE_SOCKET_CONNECTED)
		return XE_ENOTCONN;
	return loop_ -> run(req, xe_op::shutdown(fd_, how));
}

xe_promise xe_socket::shutdown(int how){
	if(state != XE_SOCKET_CONNECTED)
		return xe_promise::done(XE_ENOTCONN);
	return loop_ -> run(xe_op::shutdown(fd_, how));
}

int xe_socket::bind(sockaddr* addr, socklen_t addrlen){
	if(state != XE_SOCKET_READY)
		return XE_STATE;
	return ::bind(fd_, addr, addrlen) < 0 ? xe_errno() : 0;
}

int xe_socket::listen(int maxqueuesize){
	if(state != XE_SOCKET_READY)
		return XE_STATE;
	if(::listen(fd_, maxqueuesize) < 0)
		return xe_errno();
	state = XE_SOCKET_LISTENING;

	return 0;
}

void xe_socket::close(){
	if(fd_ >= 0){
		::close(fd_);

		fd_ = -1;
		state = XE_SOCKET_NONE;
	}
}