#pragma once
#include "../loop.h"

class xe_socket{
private:
	xe_loop& loop;

	int fd;

	uint flags;
	uint handle;
	uint pad;

	static void io(xe_loop_handle& handle, int);

	friend class xe_loop;
public:
	typedef void (*xe_callback)(xe_socket& socket, ulong key, int result);

	union{
		xe_callback accept_callback;
		xe_callback recv_callback;
	};

	xe_callback connect_callback;
	xe_callback send_callback;

	xe_socket(xe_loop& loop);

	int init(int af, int type, int proto);
	void init_fd(int fd);
	int get_fd();

	xe_loop& get_loop();

	int connect(sockaddr* addr, socklen_t addrlen, ulong key = 0);

	int recv(xe_buf buf, uint len, uint flags, ulong key = 0);
	int send(xe_buf buf, uint len, uint flags, ulong key = 0);

	int listen(int maxqueuesize);
	int bind(sockaddr* addr, socklen_t addrlen);
	int accept(sockaddr* addr, socklen_t* addrlen, uint flags);

	int cancel();

	void close();
};