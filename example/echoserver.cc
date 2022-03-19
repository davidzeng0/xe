#include <string.h>
#include <netdb.h>
#include "xe/mem.h"
#include "xe/log.h"
#include "xe/common.h"
#include "xe/loop.h"
#include "xe/io/socket.h"

static ulong t, reqs = 0, sends = 0, clients = 0;

struct client{
	xe_socket socket;
	xe_buf buf;
	uint len;

	client(xe_loop& loop): socket(loop){}
};

void client_close(client* cl){
	cl -> socket.close();

	xe_dealloc(cl -> buf);
	xe_dealloc(cl);

	clients--;

	xe_print("closing a client. %lu still open", clients);
}

void recv_callback(xe_socket& clsock, ulong unused, int result){
	client* cl = (client*)&clsock;

	if(result > 0){
		reqs++;

		clsock.send(cl -> buf, result, MSG_NOSIGNAL); /* send it back */
	}else
		client_close(cl);
}

void send_callback(xe_socket& clsock, ulong unused, int result){
	client* cl = (client*)&clsock;

	if(result > 0){
		sends++;

		clsock.recv(cl -> buf, cl -> len, 0); /* receive more */
	}else
		client_close(cl);
}

void accept_callback(xe_socket& socket, ulong unused, int result){
	if(result > 0){
		clients++;

		xe_print("accepted a client. %lu clients open", clients);

		/* create a client socket */
		client* cl = xe_znew<client>(socket.get_loop());

		cl -> len = 16384;
		cl -> buf = xe_alloc<byte>(cl -> len);

		cl -> socket.init_fd(result);
		cl -> socket.recv(cl -> buf, cl -> len, 0);

		cl -> socket.recv_callback = recv_callback;
		cl -> socket.send_callback = send_callback;

		if(socket.accept(null, null, 0) < 0)
			xe_print("loop reached io limit");
	}else
		xe_print("failed to accept");
}

void timer_callback(xe_loop& loop, xe_timer& timer){
	ulong now = xe_time_ns();

	xe_print("%lu reqs %lu sends in %f ms", reqs, sends, (now - t) / 1e6);

	t = now;
	reqs = 0;
	sends = 0;
}

int main(){
	xe_loop loop;
	xe_loop_options options;
	xe_socket socket(loop);
	xe_timer timer;

	int ret;

	options.capacity = 2048; /* sqes and cqes */

	/* init */
	ret = loop.init_options(options);

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
	ret = socket.init(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	int yes = 1;

	setsockopt(socket.get_fd(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	ret = socket.bind((sockaddr*)&addr, sizeof(addr));
	ret = socket.listen(SOMAXCONN);

	socket.accept_callback = accept_callback;

	socket.accept(null, null, 0);

	timer.callback = timer_callback;

	loop.timer_ms(timer, 1000, true);

	ret = loop.run();

	if(ret){
		xe_print("loop_run %s", strerror(-ret));

		return -1;
	}

	loop.destroy();

	return 0;
}