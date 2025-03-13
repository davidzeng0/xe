#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <xe/loop.h>
#include <xe/clock.h>
#include <xe/error.h>
#include <xe/io/socket.h>
#include <xe/io/poll.h>
#include <xutil/mem.h>
#include <xutil/log.h>
#include <xutil/endian.h>

static ulong last_time, recvs = 0, sends = 0, clients = 0;
static xe_socket server;

/* stats */
static int timer_callback(xe_loop& loop, xe_timer& timer){
	ulong now = xe_time_ns();

	xe_print("%lu reqs %lu sends in %f ms", recvs, sends, (now - last_time) / 1e6);

	last_time = now;
	recvs = 0;
	sends = 0;

	return 0;
}

/* client structure */
struct echo_client{
	static constexpr uint buffer_length = 16384;

	xe_socket socket;
	xe_poll poll;
	byte* buf;

	static void poll_cb(xe_poll& poll, int result){
		echo_client& client = xe_containerof(poll, &echo_client::poll);

		if(result < 0){
			client.close();

			return;
		}

		if(result & (XE_POLL_ERR | XE_POLL_HUP | XE_POLL_NVAL | XE_POLL_RDHUP)){
			/* some kind of socket error occurred */
			client.close();

			return;
		}

		/* we only asked for XE_POLL_IN, so we only have XE_POLL_IN (assuming we checked the err flags) */
		do{
			result = client.socket.recv_sync(client.buf, buffer_length, MSG_DONTWAIT);

			if(result <= 0)
				break;
			recvs++;
			result = client.socket.send_sync(client.buf, result, MSG_DONTWAIT | MSG_NOSIGNAL);

			if(result < 0)
				break;
			sends++;

			return;
		}while(false);

		/* error */
		client.close();
	}

	static void close_cb(xe_poll& poll){
		echo_client& client = xe_containerof(poll, &echo_client::poll);

		xe_delete(&client);
	}

	void close(){
		int result = poll.close();

		if(!result){
			/* closed immediately */
			close_cb(poll);
		}

		/* result is either XE_EINPROGRESS meaning it may close later
		 * or there is no more memory to queue the cancellation of the poll request
		 * either way, checking it is useless as there is no recovery if it fails
		 *
		 * when the loop exits due to out of memory,
		 * the application should immediately exit
		 */
	}

	echo_client(xe_loop& loop, int fd): poll(loop){
		/* set up socket */
		int yes = 1;

		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));

		socket.accept(fd);

		/* set up callbacks */
		poll.poll_callback = poll_cb;
		poll.close_callback = close_cb;

		/* alloc buffer */
		buf = xe_alloc_aligned<byte>(0, buffer_length); /* 0 = page size aligned */

		xe_print("accepted a client. %lu clients open", ++clients);

		/* start polling */
		poll.set_fd(socket.fd());
		poll.poll(XE_POLL_IN);
	}

	~echo_client(){
		socket.close();

		xe_dealloc(buf);
		xe_print("closing a client. %lu still open", --clients);
	}
};

static void accept_callback(xe_poll& poll, int result){
	if(result < 0){
		xe_print("failed to poll: %s", xe_strerror(result));

		poll.close();

		return;
	}

	if(result & XE_POLL_IN){
		int client = server.accept_sync(null, null, 0);

		if(client < 0){
			xe_print("failed to accept: %s", xe_strerror(result));

			poll.close();

			return;
		}

		/* create a client socket */
		xe_znew<echo_client>(poll.loop(), client);
	}
}

static void setup_socket(){
	int yes = 1;

	/* listen addr */
	sockaddr_in addr;

	xe_zero(&addr);

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = xe_hton<ushort>(8080);

	server.init_sync(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	setsockopt(server.fd(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	server.bind((sockaddr*)&addr, sizeof(addr));
	server.listen(SOMAXCONN);
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

	/* listen on addr */
	setup_socket();

	/* accept clients */
	xe_poll accept_poll(loop);

	accept_poll.poll_callback = accept_callback;
	accept_poll.set_fd(server.fd());
	accept_poll.poll(XE_POLL_IN);

	/* accept clients */
	last_time = xe_time_ns();
	timer.callback = timer_callback;
	loop.timer(timer, 1s, 1s, XE_TIMER_REPEAT | XE_TIMER_ALIGN);

	/* run */
	loop.run();

	/* cleanup */
	loop.close();
	server.close();

	return 0;
}