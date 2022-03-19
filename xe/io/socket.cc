#include <unistd.h>
#include "../error.h"
#include "../log.h"
#include "../common.h"
#include "../assert.h"
#include "socket.h"

enum xe_socket_flags{
	XE_SOCKET_NONE       = 0x0,
	XE_SOCKET_ACCEPTING  = 0x1,
	XE_SOCKET_CONNECTING = 0x2,
	XE_SOCKET_CONNECTED  = 0x4
};

enum XE_SOCKET_iotype{
	XE_SOCKET_CONNECT = 0,
	XE_SOCKET_ACCEPT,
	XE_SOCKET_RECV,
	XE_SOCKET_SEND,
};

xe_socket::xe_socket(xe_loop& loop): loop(loop){
	fd = -1;
	handle = -1;
	flags = 0;
}

int xe_socket::init(int af, int type, int proto){
	int fd = socket(af, type, proto);

	if(fd < 0)
		return xe_errno();
	init_fd(fd);

	return 0;
}

void xe_socket::init_fd(int fd_){
	fd = fd_;
}

int xe_socket::get_fd(){
	return fd;
}

xe_loop& xe_socket::get_loop(){
	return loop;
}

int xe_socket::connect(sockaddr* addr, socklen_t addrlen, ulong key){
	if(flags & (XE_SOCKET_CONNECTING | XE_SOCKET_ACCEPTING | XE_SOCKET_CONNECTED))
		return XE_EINVAL;
	int ret = loop.connect(fd, addr, addrlen, this, null, XE_SOCKET_CONNECT, key, XE_LOOP_HANDLE_SOCKET);

	if(ret >= 0){
		handle = ret;
		flags |= XE_SOCKET_CONNECTING;

		return 0;
	}

	return ret;
}

int xe_socket::recv(xe_buf buf, uint len, uint flags, ulong key){
	return loop.recv(fd, buf, len, flags, this, null, XE_SOCKET_RECV, key, XE_LOOP_HANDLE_SOCKET);
}

int xe_socket::send(xe_buf buf, uint len, uint flags, ulong key){
	return loop.send(fd, buf, len, flags, this, null, XE_SOCKET_SEND, key, XE_LOOP_HANDLE_SOCKET);
}

int xe_socket::bind(sockaddr* addr, socklen_t addrlen){
	if(::bind(fd, addr, addrlen) < 0)
		return xe_errno();
	return 0;
}

int xe_socket::listen(int maxqueuesize){
	if(::listen(fd, maxqueuesize) < 0)
		return xe_errno();
	return 0;
}

int xe_socket::accept(sockaddr* addr, socklen_t* addrlen, uint flags){
	if(flags & (XE_SOCKET_CONNECTING | XE_SOCKET_CONNECTED | XE_SOCKET_ACCEPTING))
		return XE_EINVAL;
	int ret = loop.accept(fd, addr, addrlen, flags, this, null, XE_SOCKET_ACCEPT, 0, XE_LOOP_HANDLE_SOCKET);

	if(ret >= 0){
		handle = ret;
		flags |= XE_SOCKET_ACCEPTING;

		return 0;
	}

	return ret;
}

int xe_socket::cancel(){
	if(!(flags & (XE_SOCKET_ACCEPTING | XE_SOCKET_CONNECTING)))
		return XE_EINVAL;
	int ret = loop.cancel(handle, 0, null, null, 0, 0, XE_LOOP_HANDLE_DISCARD);

	if(ret > 0)
		ret = 0;
	return ret;
}

void xe_socket::close(){
	::close(fd);

	fd = -1;
}

void xe_socket::io(xe_loop_handle& handle, int result){
	xe_socket& socket = *(xe_socket*)handle.user_data;

	switch(handle.u1){
		case XE_SOCKET_CONNECT:
			socket.flags &= ~XE_SOCKET_CONNECTING;

			if(!result)
				socket.flags |= XE_SOCKET_CONNECTED;
			if(!socket.connect_callback)
				break;
			socket.connect_callback(socket, 0, result);

			break;
		case XE_SOCKET_ACCEPT:
			socket.flags &= ~XE_SOCKET_ACCEPTING;

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
	}
}