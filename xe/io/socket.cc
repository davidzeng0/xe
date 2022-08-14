#include <unistd.h>
#include "socket.h"
#include "xutil/assert.h"
#include "../error.h"

enum XE_SOCKET_iotype{
	XE_SOCKET_CONNECT = 0,
	XE_SOCKET_ACCEPT,
	XE_SOCKET_RECV,
	XE_SOCKET_SEND,
};

xe_socket::xe_socket(xe_loop& loop): loop_(loop){
	fd_ = -1;
	handle = -1;
	accepting = 0;
	connecting = 0;
	connected = 0;
	flags = 0;
}

int xe_socket::init(int af, int type, int proto){
	int fd = socket(af, type, proto);

	if(fd < 0)
		return xe_errno();
	init_fd(fd);

	return 0;
}

void xe_socket::init_fd(int fd){
	fd_ = fd;
}

int xe_socket::fd(){
	return fd_;
}

xe_loop& xe_socket::loop(){
	return loop_;
}

int xe_socket::connect(sockaddr* addr, socklen_t addrlen, ulong key){
	if(connecting)
		return XE_EALREADY;
	if(accepting || connected)
		return XE_STATE;
	int ret = loop_.connect(fd_, addr, addrlen, this, null, XE_SOCKET_CONNECT, key, XE_LOOP_HANDLE_SOCKET);

	if(ret >= 0){
		handle = ret;
		connecting = true;

		return 0;
	}

	return ret;
}

int xe_socket::recv(xe_ptr buf, uint len, uint flags, ulong key){
	return loop_.recv(fd_, buf, len, flags, this, null, XE_SOCKET_RECV, key, XE_LOOP_HANDLE_SOCKET);
}

int xe_socket::send(xe_cptr buf, uint len, uint flags, ulong key){
	return loop_.send(fd_, buf, len, flags, this, null, XE_SOCKET_SEND, key, XE_LOOP_HANDLE_SOCKET);
}

int xe_socket::bind(sockaddr* addr, socklen_t addrlen){
	return ::bind(fd_, addr, addrlen) < 0 ? xe_errno() : 0;
}

int xe_socket::listen(int maxqueuesize){
	return ::listen(fd_, maxqueuesize) < 0 ? xe_errno() : 0;
}

int xe_socket::accept(sockaddr* addr, socklen_t* addrlen, uint flags){
	if(accepting)
		return XE_EALREADY;
	if(connecting || connected)
		return XE_STATE;
	int ret = loop_.accept(fd_, addr, addrlen, flags, this, null, XE_SOCKET_ACCEPT, 0, XE_LOOP_HANDLE_SOCKET);

	if(ret >= 0){
		handle = ret;
		accepting = true;

		return 0;
	}

	return ret;
}

int xe_socket::cancel(){
	if(!accepting && !connecting)
		return XE_ENOENT;
	int ret = loop_.cancel(handle, 0, null, null, 0, 0, XE_LOOP_HANDLE_DISCARD);

	return ret > 0 ? 0 : ret;
}

void xe_socket::close(){
	if(fd_ >= 0){
		::close(fd_);

		fd_ = -1;
	}
}

void xe_socket::io(xe_loop_handle& handle, int result){
	xe_socket& socket = *(xe_socket*)handle.user_data;

	switch(handle.u1){
		case XE_SOCKET_CONNECT:
			socket.connecting = false;

			if(!result)
				socket.connected = true;
			if(!socket.connect_callback)
				break;
			socket.connect_callback(socket, 0, result);

			break;
		case XE_SOCKET_ACCEPT:
			socket.accepting = false;

			if(!socket.accept_callback)
				break;
			socket.accept_callback(socket, 0, result);

			break;
		case XE_SOCKET_RECV:
			if(!socket.recv_callback)
				break;
			socket.recv_callback(socket, handle.u2, result);

			break;
		case XE_SOCKET_SEND:
			if(!socket.send_callback)
				break;
			socket.send_callback(socket, handle.u2, result);

			break;
		default:
			xe_notreached();

			break;
	}
}