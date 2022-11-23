#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <linux/version.h>
#include <xe/loop.h>
#include <xe/clock.h>
#include <xe/error.h>
#include <xe/io/socket.h>
#include <xutil/mem.h>
#include <xutil/log.h>
#include <xutil/endian.h>
#include "coroutine.h"

static ulong last_time, recvs = 0, sends = 0, clients = 0;

static int timer_callback(xe_loop& loop, xe_timer& timer){
	ulong now = xe_time_ns();

	xe_print("%lu recvs %lu sends in %f ms", recvs, sends, (now - last_time) / 1e6);

	last_time = now;
	recvs = 0;
	sends = 0;

	return 0;
}

static task echo(xe_loop& loop, int fd){
	/* smaller buffer sizes yield greater performance due to close proximity between blocks */
	constexpr uint buffer_length = 512;

	xe_socket socket(loop);
	int result;
	byte* buf;

	buf = xe_alloc_aligned<byte>(buffer_length, buffer_length);
	socket.accept(fd);

	while(true){
		result = co_await socket.recv(buf, buffer_length, 0);

		if(result <= 0)
			break;
		recvs++;
		result = co_await socket.send(buf, result, 0);

		if(result < 0)
			break;
		sends++;
	}

	socket.close();

	xe_dealloc(buf);
	xe_print("client closed with error: %s, %lu still open", xe_strerror(result), --clients);
}

static task start_server(xe_loop& loop){
	/* listen addr */
	sockaddr_in addr;
	int yes = 1;

	xe_zero(&addr);

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = xe_hton<ushort>(8080);

	xe_socket server(loop);

	/* create a socket */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0)
	server.init_sync(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
	co_await server.init(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif

	setsockopt(server.fd(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	server.bind((sockaddr*)&addr, sizeof(sockaddr));
	server.listen(SOMAXCONN);

	int client;

	/* accept clients */
	while(true){
		client = co_await server.accept(null, null, 0);

		if(client < 0)
			break;
		setsockopt(client, SOL_SOCKET, TCP_NODELAY, &yes, sizeof(yes));
		echo(loop, client);
		xe_print("accepted a client. %lu clients open", ++clients);
	}

	server.close();

	xe_print("could not accept client: %s", xe_strerror(client));
}

int main(){
	using namespace std::chrono_literals;

	xe_loop loop;
	xe_loop_options options;
	xe_timer timer;

	options.entries = 256; /* number of sqes, seems to work the best */
	options.cq_entries = 65536;
	options.flag_cqsize = true;

	/* init */
	loop.init_options(options);

	xe_print("initialized with %u sqes and %u cqes", loop.sqe_count(), loop.cqe_count());

	/* start */
	start_server(loop);

	/* stats */
	timer.callback = timer_callback;
	loop.timer(timer, 1s, 1s, XE_TIMER_REPEAT | XE_TIMER_ALIGN);
	last_time = xe_time_ns();

	loop.run();

	/* cleanup */
	loop.close();

	return 0;
}