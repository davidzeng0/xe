#include <netdb.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "xutil/log.h"
#include "xutil/mem.h"
#include "xutil/endian.h"
#include "xe/loop.h"
#include "xe/clock.h"
#include "xe/error.h"
#include "xe/io/socket.h"

/* coroutine task structure */
struct task{
	struct promise_type{
		task get_return_object(){
			return task(std::coroutine_handle<promise_type>::from_promise(*this));
		}

		auto initial_suspend(){
			return std::suspend_never();
		}

		auto final_suspend() noexcept{
			return std::suspend_never();
		}

		void return_void(){}

		void unhandled_exception(){}
	};

	std::coroutine_handle<promise_type> handle;

	task(std::coroutine_handle<promise_type> h){
		handle = h;
	}

	~task(){
		/* don't kill the coroutine, nothing to do */
	}
};

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
	const int len = 16384;

	byte* buf = xe_alloc_aligned<byte>(0, len); /* page size aligned */
	int result;

	xe_socket socket(loop);

	socket.accept(fd);

	while(true){
		result = co_await socket.recv(buf, len, 0);

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
	/* create a socket */
	/* listen addr */
	sockaddr_in addr;
	int yes = 1;

	xe_zero(&addr);

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = xe_htons(8080);

	xe_socket server(loop);

	co_await server.init_async(AF_INET, SOCK_STREAM, IPPROTO_TCP);

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
	loop.timer_ms(timer, 1000, 1000, XE_TIMER_REPEAT | XE_TIMER_ALIGN);
	last_time = xe_time_ns();

	loop.run();

	/* cleanup */
	loop.close();

	return 0;
}