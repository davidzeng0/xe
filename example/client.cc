#include <string.h>
#include <netdb.h>
#include "xutil/mem.h"
#include "xutil/log.h"
#include "xutil/util.h"
#include "xstd/types.h"
#include "xstd/string.h"
#include "xe/loop.h"
#include "xe/io/socket.h"
#include "xe/error.h"

static uint len = 16384;
static byte* buf = xe_alloc<byte>(len);

static xe_string_view msg = "Hello World!";

void recv_callback(xe_socket& socket, ulong unused, int result){
	if(result > 0)
		xe_print("%.*s", result, buf);
}

void send_callback(xe_socket& socket, ulong unused, int result){
	if(result > 0){
		xe_print("sent %d bytes", result);

		socket.recv(buf, len, 0);
	}
}

void connect_callback(xe_socket& socket, ulong unused, int result){
	if(!result){
		xe_print("connected");

		socket.send(msg.data(), msg.size(), MSG_NOSIGNAL);
	}
}

int main(){
	xe_loop loop;
	xe_loop_options options;
	xe_socket socket(loop);

	int ret;

	options.capacity = 8; /* sqes and cqes */

	/* init */
	ret = loop.init_options(options);

	if(ret){
		xe_print("loop_init %s", xe_strerror(ret));

		return -1;
	}

	/* connect address */
	sockaddr_in addr;

	xe_zero(&addr);

	addr.sin_addr.s_addr = htonl(0x7f000001); /* 127.0.0.1 */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);

	/* nonzero ret is a negative system error */
	ret = socket.init(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	ret = socket.connect((sockaddr*)&addr, sizeof(addr));

	socket.connect_callback = connect_callback;
	socket.recv_callback = recv_callback;
	socket.send_callback = send_callback;

	ret = loop.run();

	if(ret){
		xe_print("loop_run %s", xe_strerror(ret));

		return -1;
	}

	loop.close();

	return 0;
}