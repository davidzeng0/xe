#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <xe/loop.h>
#include <xe/clock.h>
#include <xe/error.h>
#include <xe/io/socket.h>
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
	/* smaller buffer sizes yield greater performance due to close proximity between blocks */
	static constexpr uint buffer_length = 512;

	xe_socket socket;
	xe_req recv;
	xe_req send;
	byte* buf;

	static void recv_callback(xe_req& req, int result){
		echo_client& client = xe_containerof(req, &echo_client::recv);

		if(result > 0){
			recvs++;
			client.socket.send(client.send, client.buf, result, 0); /* send it back */
		}else{
			/* error */
			xe_delete(&client);
		}
	}

	static void send_callback(xe_req& req, int result){
		echo_client& client = xe_containerof(req, &echo_client::send);

		if(result > 0){
			sends++;
			client.socket.recv(client.recv, client.buf, buffer_length, 0); /* receive more */
		}else{
			/* error */
			xe_delete(&client);
		}
	}

	echo_client(xe_loop& loop, int fd): socket(loop){
		/* set up socket */
		int yes = 1;

		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));

		socket.accept(fd);

		/* set up callbacks */
		recv.callback = recv_callback;
		send.callback = send_callback;

		/* alloc buffer */
		buf = xe_alloc_aligned<byte>(buffer_length, buffer_length);

		xe_print("accepted a client. %lu clients open", ++clients);

		/* start recving */
		socket.recv(recv, buf, buffer_length, 0);
	}

	~echo_client(){
		socket.close();

		xe_dealloc(buf);
		xe_print("closing a client. %lu still open", --clients);
	}
};

static void accept_callback(xe_req& req, int result){
	if(result < 0){
		xe_print("failed to accept: %s", xe_strerror(result));

		return;
	}

	/* queue up another accept */
	server.accept(req, null, null, 0);

	/* create a client socket */
	xe_znew<echo_client>(server.loop(), result);
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
	xe_req accept_req;

	accept_req.callback = accept_callback;
	server.set_loop(loop);
	server.accept(accept_req, null, null, 0);

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