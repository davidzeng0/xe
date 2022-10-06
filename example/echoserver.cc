#include <netdb.h>
#include <arpa/inet.h>
#include "xe/loop.h"
#include "xe/io/socket.h"
#include "xe/clock.h"
#include "xe/error.h"
#include "xutil/mem.h"
#include "xutil/log.h"
#include "xutil/endian.h"

static ulong last_time, recvs = 0, sends = 0, clients = 0;

void recv_callback(xe_req& req, int result);
void send_callback(xe_req& req, int result);

struct client{
	xe_socket socket;
	xe_req recv;
	xe_req send;
	byte* buf;
	uint len;

	client(xe_loop& loop): socket(loop){
		recv.callback = recv_callback;
		send.callback = send_callback;
	}

	~client(){
		socket.close();

		xe_dealloc(buf);
	}
};

void client_close(client& cl){
	xe_delete(&cl);
	xe_print("closing a client. %lu still open", --clients);
}

void recv_callback(xe_req& req, int result){
	client& cl = xe_containerof(req, &client::recv);

	if(result > 0){
		recvs++;
		cl.socket.send(cl.send, cl.buf, result, 0); /* send it back */
	}else{
		client_close(cl);
	}
}

void send_callback(xe_req& req, int result){
	client& cl = xe_containerof(req, &client::send);

	if(result > 0){
		sends++;
		cl.socket.recv(cl.recv, cl.buf, cl.len, 0); /* receive more */
	}else{
		client_close(cl);
	}
}

class accept_req : public xe_req{
public:
	xe_socket& socket;

	accept_req(xe_socket& socket): socket(socket){}
};

void accept_callback(xe_req& req, int result){
	xe_socket& socket = ((accept_req&)req).socket;

	if(result > 0){
		xe_print("accepted a client. %lu clients open", ++clients);

		/* create a client socket */
		client* cl = xe_znew<client>(socket.loop());

		cl -> len = 16384;
		cl -> buf = xe_alloc_aligned<byte>(0, cl -> len); /* page size aligned alloc */

		cl -> socket.accept(result);
		cl -> socket.recv(cl -> recv, cl -> buf, cl -> len, 0);

		socket.accept(req, null, null, 0);
	}else{
		xe_print("failed to accept: %s", xe_strerror(result));
	}
}

int timer_callback(xe_loop& loop, xe_timer& timer){
	ulong now = xe_time_ns();

	xe_print("%lu reqs %lu sends in %f ms", recvs, sends, (now - last_time) / 1e6);

	last_time = now;
	recvs = 0;
	sends = 0;

	return 0;
}

int main(){
	xe_loop loop;
	xe_loop_options options;
	xe_socket socket(loop);
	xe_timer timer;

	int ret;

	options.entries = 256; /* number of sqes, seems to work the best */
	options.cq_entries = 65536;
	options.flag_cqsize = true;

	/* init */
	loop.init_options(options);

	/* listen addr */
	sockaddr_in addr;

	xe_zero(&addr);

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = xe_htons(8080);

	socket.init(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	int yes = 1;

	setsockopt(socket.fd(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	socket.bind((sockaddr*)&addr, sizeof(addr));
	socket.listen(SOMAXCONN);

	accept_req req(socket);

	req.callback = accept_callback;
	socket.accept(req, null, null, 0);

	last_time = xe_time_ns();
	timer.callback = timer_callback;

	loop.timer_ms(timer, 1000, 1000, XE_TIMER_REPEAT | XE_TIMER_ALIGN);
	loop.run();

	loop.close();

	return 0;
}