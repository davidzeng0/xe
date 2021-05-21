#pragma once
#include <netdb.h>
#include "loop.h"

struct xe_socket{
	typedef void (*xe_callback)(xe_socket* socket, ulong key, int result);

	xe_loop* loop;

	int fd;

	uint flags;
	uint cancel;
	uint pad;

	union{
		xe_callback accept_callback;
		xe_callback recv_callback;
	};

	xe_callback connect_callback;
	xe_callback send_callback;
};

int xe_socket_init(xe_loop* loop, xe_socket* socket, int af, int type, int proto);
void xe_socket_init_fd(xe_loop* loop, xe_socket* socket, int fd);

int xe_socket_connect(xe_socket* socket, sockaddr* addr, socklen_t addrlen);

int xe_socket_recv(xe_socket* socket, xe_buf buf, uint len, uint flags);
int xe_socket_send(xe_socket* socket, xe_buf buf, uint len, uint flags);

int xe_socket_recv(xe_socket* socket, xe_buf buf, uint len, uint flags, ulong key);
int xe_socket_send(xe_socket* socket, xe_buf buf, uint len, uint flags, ulong key);

int xe_socket_bind(xe_socket* socket, sockaddr* addr, socklen_t addrlen);
int xe_socket_listen(xe_socket* socket, int maxqueuesize);
int xe_socket_accept(xe_socket* socket, sockaddr* addr, socklen_t* addrlen, uint flags);

int xe_socket_cancel(xe_socket* socket);

void xe_socket_close(xe_socket* socket);

void xe_loop_socket(xe_socket*, xe_handle*, int);