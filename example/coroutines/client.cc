#include <netdb.h>
#include <arpa/inet.h>
#include <linux/version.h>
#include <xstd/string.h>
#include <xe/loop.h>
#include <xe/error.h>
#include <xe/io/socket.h>
#include <xutil/mem.h>
#include <xutil/log.h>
#include <xutil/endian.h>
#include "coroutine.h"

static task run(xe_loop& loop){
	const uint buffer_length = 16384;

	byte* buf = xe_alloc<byte>(buffer_length);
	xe_string_view msg = "Hello World!";
	int recvd;

	xe_socket client(loop);

	/* connect address */
	sockaddr_in addr;

	xe_zero(&addr);

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = xe_hton<ushort>(8080);

	/* create a socket */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0)
	client.init_sync(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
	co_await client.init(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
	co_await client.connect((sockaddr*)&addr, sizeof(addr));

	co_await client.send(msg.data(), msg.length(), 0);

	recvd = co_await client.recv(buf, buffer_length, 0);

	if(recvd > 0)
		xe_print("incoming message: %.*s", recvd, buf);
	else
		xe_print("failed to recv: %s", xe_strerror(recvd));
}

int main(){
	xe_loop loop;

	/* init */
	loop.init(8); /* 8 sqes and cqes */

	run(loop);

	loop.run();

	/* cleanup */
	loop.close();

	return 0;
}