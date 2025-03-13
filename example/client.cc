#include <netdb.h>
#include <arpa/inet.h>
#include <xstd/string.h>
#include <xe/loop.h>
#include <xe/error.h>
#include <xe/io/socket.h>
#include <xutil/mem.h>
#include <xutil/log.h>
#include <xutil/endian.h>

static const uint buffer_length = 16384;
static byte* buf = xe_alloc<byte>(buffer_length);

static xe_string_view msg = "Hello World!";

struct client{
	xe_socket socket;
	xe_connect_req conn;
	xe_req recv;
	xe_req send;
};

void connect_callback(xe_connect_req& req, int result){
	client& client = xe_containerof(req, &client::conn);

	xe_print("connect status: %s", xe_strerror(result));

	if(!result)
		client.socket.send(client.send, msg.data(), msg.length(), 0);
}

void recv_callback(xe_req& req, int result){
	xe_print("recv status: %s", xe_strerror(xe_min(result, 0)));

	if(result > 0)
		xe_print("incoming message: %.*s", result, buf);
}

void send_callback(xe_req& req, int result){
	client& client = xe_containerof(req, &client::send);

	xe_print("send status: %s", xe_strerror(xe_min(result, 0)));

	if(result > 0)
		client.socket.recv(client.recv, buf, buffer_length, 0);
}

int main(){
	xe_loop loop;
	client c;

	/* init */
	loop.init(8); /* 8 sqes and cqes */

	/* connect address */
	sockaddr_in addr;

	xe_zero(&addr);

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = xe_hton<ushort>(8080);

	c.socket.set_loop(loop);
	c.conn.callback = connect_callback;
	c.recv.callback = recv_callback;
	c.send.callback = send_callback;

	/* connect */
	c.socket.init_sync(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	c.socket.connect(c.conn, (sockaddr*)&addr, sizeof(addr));

	loop.run();

	loop.close();

	return 0;
}