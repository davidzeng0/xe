#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include "xe/common.h"
#include "xe/debug.h"
#include "xe/mem.h"
#include "xe/error.h"
#include "loop.h"
#include "async.h"

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

	~task(){}
};

static ulong t, reqs = 0, sends = 0, clients = 0;

void timer_callback(xe_loop* loop, xe_timer* timer){
	ulong now = xe_time_ns();

	xe_print("%lu reqs %lu sends in %f ms", reqs, sends, (now - t) / 1e6);

	t = now;
	reqs = 0;
	sends = 0;
}

static task echo(xe_async& async, int fd){
	uint len = 16384;
	xe_buf buf = xe_alloc<byte>(len);

	while(true){
		int bytes_recvd = co_await async.recv(fd, buf, len, 0);

		if(bytes_recvd <= 0){
			if(bytes_recvd == 0)
				break;
			if(bytes_recvd == XE_ETOOMANYHANDLES)
				xe_print("loop reached io limit");
			else
				xe_print("could not recv");
			break;
		}

		reqs++;

		int bytes_sent = co_await async.send(fd, buf, bytes_recvd, MSG_NOSIGNAL);

		if(bytes_sent <= 0){
			xe_print("could not send");

			break;
		}

		sends++;
	}

	xe_print("closing a client, %lu still open", --clients);
	xe_dealloc(buf);
	close(fd);
}

static task accept_connections(xe_async& async, int fd){
	while(true){
		int client = co_await async.accept(fd, null, null, 0);

		if(client < 0){
			if(client == XE_ETOOMANYHANDLES)
				xe_print("loop reached io limit");
			else
				xe_print("could not accept client");
			break;
		}

		xe_print("accepted a client. %lu clients open", ++clients);
		echo(async, client);
	}

	close(fd);
}

int main(){
	xe_loop loop;
	xe_loop_options options;
	xe_timer timer;

	int ret;

	/* xe lib expects all structs to be zeroed */
	xe_zero(&options);
	xe_zero(&loop);
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

	/* create a socket */

	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int yes = 1;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	bind(fd, (sockaddr*)&addr, sizeof(sockaddr));
	listen(fd, SOMAXCONN);

	xe_async async(&loop);

	accept_connections(async, fd); /* accept clients */

	/* stats */

	timer.callback = timer_callback;
	t = xe_time_ns();

	xe_loop_timer_ms(&loop, &timer, 1000, true);

	ret = xe_loop_run(&loop);

	if(ret){
		xe_print("loop_run %s", strerror(-ret));

		return -1;
	}

	xe_loop_destroy(&loop);

	return 0;
}