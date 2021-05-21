#include <string.h>
#include "xe/mem.h"
#include "xe/debug.h"
#include "xe/common.h"
#include "loop.h"
#include "socket.h"

static ulong t, reqs = 0, sends = 0, clients = 0;

struct client{
	xe_socket socket;
	xe_buf buf;
	uint len;
};

void client_close(client* cl){
	xe_socket_close(&cl -> socket);
	xe_dealloc(cl -> buf);
	xe_dealloc(cl);

	clients--;

	xe_print("closing a client. %lu still open", clients);
}

void recv_callback(xe_socket* clsock, ulong unused, int result){
	client* cl = (client*)clsock;

	if(result > 0){
		reqs++;

		xe_socket_send(clsock, cl -> buf, result, MSG_NOSIGNAL); /* send it back */
	}else
		client_close(cl);
}

void send_callback(xe_socket* clsock, ulong unused, int result){
	client* cl = (client*)clsock;

	if(result > 0){
		sends++;

		xe_socket_recv(clsock, cl -> buf, cl -> len, 0); /* receive more */
	}else
		client_close(cl);
}

void accept_callback(xe_socket* socket, ulong unused, int result){
	if(result > 0){
		clients++;

		xe_print("accepted a client. %lu clients open", clients);

		/* create a client socket */
		client* cl = xe_zalloc<client>();

		cl -> len = 16384;
		cl -> buf = xe_alloc<byte>(cl -> len);

		xe_socket_init_fd(socket -> loop, &cl -> socket, result);
		xe_socket_recv(&cl -> socket, cl -> buf, cl -> len, 0);

		cl -> socket.recv_callback = recv_callback;
		cl -> socket.send_callback = send_callback;

		if(xe_socket_accept(socket, null, null, 0) < 0)
			xe_print("loop reached io limit");
	}else
		xe_print("failed to accept");
}

void timer_callback(xe_loop* loop, xe_timer* timer){
	ulong now = xe_time_ns();

	xe_print("%lu reqs %lu sends in %f ms", reqs, sends, (now - t) / 1e6);

	t = now;
	reqs = 0;
	sends = 0;
}

int main(){
	xe_loop loop;
	xe_loop_options options;
	xe_socket socket;
	xe_timer timer;

	int ret;

	/* xe lib expects all structs to be zeroed */
	xe_zero(&options);
	xe_zero(&loop);
	xe_zero(&socket);
	xe_zero(&timer);

	options.capacity = 512; /* 512 sqes and cqes */

	/* init */
	ret = xe_loop_init_options(&loop, &options);

	if(ret){
		xe_print("loop_init %s", strerror(-ret));

		return -1;
	}

	/* listen addr */
	sockaddr_in addr;

	xe_zero(&addr);

	addr.sin_addr.s_addr = htonl(0x7f000001); /* 127.0.0.1 */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);

	/* nonzero ret is a negative system error */
	ret = xe_socket_init(&loop, &socket, AF_INET, SOCK_STREAM, IPPROTO_TCP);

	int yes = 1;

	setsockopt(socket.fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	ret = xe_socket_bind(&socket, (sockaddr*)&addr, sizeof(addr));
	ret = xe_socket_listen(&socket, SOMAXCONN);

	socket.accept_callback = accept_callback;

	xe_socket_accept(&socket, null, null, 0);

	timer.callback = timer_callback;

	xe_loop_timer_ms(&loop, &timer, 1000, true);

	ret = xe_loop_run(&loop);

	if(ret){
		xe_print("loop_run %s", strerror(-ret));

		return -1;
	}

	xe_loop_destroy(&loop);

	return 0;
}