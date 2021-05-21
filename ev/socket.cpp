#include <unistd.h>
#include "xe/error.h"
#include "xe/debug.h"
#include "xe/common.h"
#include "socket.h"

enum xe_socket_flags{
	XE_SOCKET_NONE       = 0x0,
	XE_SOCKET_ACCEPTING  = 0x1,
	XE_SOCKET_CONNECTING = 0x2,
	XE_SOCKET_CONNECTED  = 0x4
};

enum xe_socket_iotype{
	XE_SOCKET_CONNECT = 0,
	XE_SOCKET_ACCEPT,
	XE_SOCKET_RECV,
	XE_SOCKET_SEND,
};

int xe_socket_init(xe_loop* loop, xe_socket* sock, int af, int type, int proto){
	int fd = socket(af, type, proto);

	if(fd < 0)
		return -errno;
	xe_socket_init_fd(loop, sock, fd);

	return 0;
}

void xe_socket_init_fd(xe_loop* loop, xe_socket* socket, int fd){
	socket -> loop = loop;
	socket -> fd = fd;
}

int xe_socket_connect(xe_socket* socket, sockaddr* addr, socklen_t addrlen){
	if(socket -> flags & (XE_SOCKET_CONNECTING | XE_SOCKET_ACCEPTING | XE_SOCKET_CONNECTED))
		return XE_EINVAL;
	int ret = xe_loop_connect(socket -> loop, socket -> fd, addr, addrlen, XE_LOOP_SOCKET, socket, null, XE_SOCKET_CONNECT, 0);

	if(ret >= 0){
		socket -> cancel = ret;
		socket -> flags |= XE_SOCKET_CONNECTING;

		return 0;
	}

	return ret;
}

int xe_socket_recv(xe_socket* socket, xe_buf buf, uint len, uint flags){
	return xe_socket_recv(socket, buf, len, flags, 0);
}

int xe_socket_send(xe_socket* socket, xe_buf buf, uint len, uint flags){
	return xe_socket_send(socket, buf, len, flags, 0);
}

int xe_socket_recv(xe_socket* socket, xe_buf buf, uint len, uint flags, ulong key){
	return xe_loop_recv(socket -> loop, socket -> fd, buf, len, flags, XE_LOOP_SOCKET, socket, null, XE_SOCKET_RECV, key);
}

int xe_socket_send(xe_socket* socket, xe_buf buf, uint len, uint flags, ulong key){
	return xe_loop_send(socket -> loop, socket -> fd, buf, len, flags, XE_LOOP_SOCKET, socket, null, XE_SOCKET_SEND, key);
}

int xe_socket_bind(xe_socket* socket, sockaddr* addr, socklen_t addrlen){
	if(bind(socket -> fd, addr, addrlen) < 0)
		return -errno;
	return 0;
}

int xe_socket_listen(xe_socket* socket, int maxqueuesize){
	if(listen(socket -> fd, maxqueuesize) < 0)
		return -errno;
	return 0;
}

int xe_socket_accept(xe_socket* socket, sockaddr* addr, socklen_t* addrlen, uint flags){
	if(socket -> flags & (XE_SOCKET_CONNECTING | XE_SOCKET_CONNECTED | XE_SOCKET_ACCEPTING))
		return XE_EINVAL;
	int ret = xe_loop_accept(socket -> loop, socket -> fd, addr, addrlen, flags, XE_LOOP_SOCKET, socket, null, XE_SOCKET_ACCEPT, 0);

	if(ret >= 0){
		socket -> cancel = ret;
		socket -> flags |= XE_SOCKET_ACCEPTING;

		return 0;
	}

	return ret;
}

int xe_socket_cancel(xe_socket* socket){
	if(!(socket -> flags & (XE_SOCKET_ACCEPTING | XE_SOCKET_CONNECTING)))
		return XE_EINVAL;
	int ret = xe_loop_cancel(socket -> loop, socket -> cancel, 0, XE_LOOP_DISCARD, null, null, 0, 0);

	if(ret > 0)
		ret = 0;
	return ret;
}

void xe_socket_close(xe_socket* socket){
	close(socket -> fd);

	socket -> fd = -1;
}

void xe_loop_socket(xe_socket* socket, xe_handle* handle, int result){
	switch(handle -> u1){
		case XE_SOCKET_CONNECT:
			socket -> flags &= ~XE_SOCKET_CONNECTING;

			if(!result)
				socket -> flags |= XE_SOCKET_CONNECTED;
			socket -> connect_callback(socket, 0, result);

			break;
		case XE_SOCKET_ACCEPT:
			socket -> flags &= ~XE_SOCKET_ACCEPTING;
			socket -> accept_callback(socket, 0, result);

			break;
		case XE_SOCKET_RECV:
			socket -> recv_callback(socket, handle -> u2, result);

			break;
		case XE_SOCKET_SEND:
			socket -> send_callback(socket, handle -> u2, result);

			break;
		default:
			xe_notreached;
	}
}