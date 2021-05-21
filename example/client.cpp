#include <string.h>
#include "xe/mem.h"
#include "xe/debug.h"
#include "xe/common.h"
#include "loop.h"
#include "socket.h"

static uint len = 16384;
static xe_buf buf = xe_alloc<byte>(len);

static xe_buf msg = (xe_buf)"Hello World!";
static uint msg_len = strlen((xe_cstr)msg);

void recv_callback(xe_socket* socket, ulong unused, int result){
	if(result > 0)
		xe_print("%.*s", result, buf);
}

void send_callback(xe_socket* socket, ulong unused, int result){
	if(result > 0){
		xe_print("sent %d bytes", result);
		xe_socket_recv(socket, buf, len, 0);
	}
}

void connect_callback(xe_socket* socket, ulong unused, int result){
	if(!result){
		xe_print("connected");
		xe_socket_send(socket, msg, msg_len, MSG_NOSIGNAL);
	}
}

int main(){
	xe_loop loop;
	xe_loop_options options;
	xe_socket socket;

	int ret;

	/* xe lib expects all structs to be zeroed */
	xe_zero(&options);
	xe_zero(&loop);
	xe_zero(&socket);

	options.capacity = 8; /* 8 sqes and cqes */

	/* init */
	ret = xe_loop_init_options(&loop, &options);

	if(ret){
		xe_print("loop_init %s", strerror(-ret));

		return -1;
	}

	/* connect address */
	sockaddr_in addr;

	xe_zero(&addr);

	addr.sin_addr.s_addr = htonl(0x7f000001); /* 127.0.0.1 */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);

	/* nonzero ret is a negative system error */
	ret = xe_socket_init(&loop, &socket, AF_INET, SOCK_STREAM, IPPROTO_TCP);
	ret = xe_socket_connect(&socket, (sockaddr*)&addr, sizeof(addr));

	socket.connect_callback = connect_callback;
	socket.recv_callback = recv_callback;
	socket.send_callback = send_callback;

	ret = xe_loop_run(&loop);

	if(ret){
		xe_print("loop_run %s", strerror(-ret));

		return -1;
	}

	xe_loop_destroy(&loop);

	return 0;
}