#include <ares.h>
#include <sys/epoll.h>
#include <unistd.h>
#include "net.h"
#include "url.h"
#include "proto/http.h"
#include "proto/ws.h"
#include "proto/file.h"
#include "../mem.h"
#include "../error.h"
#include "../log.h"

using namespace xe_net;

enum xe_net_constants{
	MAX_EVENTS = 256
};

enum xe_net_flags{
	FLAG_NONE = 0x0,
	FLAG_ACTIVE = 0x1,
	FLAG_CLOSING = 0x2
};

int xe_net_shared::init(){
	int err;

	err = resolve.init();

	if(err)
		return err;
	err = ssl.init();

	if(err)
		resolve.close();
	return err;
}

int xe_net_shared::init_ssl(xe_cstr cafile, xe_cstr capath){
	int err;

	err = resolve.init();

	if(err)
		return err;
	err = ssl.init();

	if(err){
		resolve.close();

		return err;
	}

	err = ssl.load_verify_locations(cafile, capath);

	if(err){
		resolve.close();
		ssl.close();
	}

	return err;
}

void xe_net_shared::free(){
	resolve.close();
	ssl.close();
}

static void xe_net_poll(xe_net_ctx& net){
	if(net.handle != -1 || !(net.flags & FLAG_ACTIVE))
		return;
	net.loop.release(1);
	net.handle = net.loop.poll(net.pollfd, EPOLLIN, &net, null, 0, 0, XE_NET_CONNECTION);

	xe_assert(net.handle >= 0);
}

static xe_protocol* allocate_protocol(xe_net_ctx& net, int id){
	switch(id){
		case XE_PROTOCOL_FILE:
			return xe_file_new(net);
		case XE_PROTOCOL_HTTP:
			return xe_http_new(net);
		case XE_PROTOCOL_WEBSOCKET:
			return xe_websocket_new(net);
		default:
			return null;
	}
}

int xe_net_ctx::resolve(xe_connection& conn, xe_string& host, xe_endpoint& ep){
	auto entry = endpoints.find(host);

	if(entry != endpoints.end()){
		if(entry -> second.pending){
			conn.next = entry -> second.pending;

			if(conn.next)
				conn.next -> prev = &conn;
		}else{
			ep = entry -> second.endpoint;

			return 0;
		}
	}else{
		xe_string host_copy;

		int err;

		if(!host_copy.copy(host))
			return XE_ENOMEM;
		host = host_copy;
		entry = endpoints.insert(host);

		if(entry == endpoints.end()){
			host.free();

			return XE_ENOMEM;
		}

		entry -> second.pending = null;
		conn.next = null;
		err = resolver.resolve(*this, host_copy, ep);

		if(err != XE_EINPROGRESS){
			if(err)
				endpoints.erase(entry);
			else
				entry -> second.endpoint = ep;
			return err;
		}
	}

	conn.prev = null;
	entry -> second.pending = &conn;

	return XE_EINPROGRESS;
}

void xe_net_ctx::resolved(xe_string host, xe_endpoint endpoint, int status){
	xe_connection* conn;
	xe_connection* next;

	int err;
	auto entry = endpoints.find(host);

	xe_assert(entry != endpoints.end());

	conn = entry -> second.pending;
	entry -> second.pending = null;

	if(!status)
		entry -> second.endpoint = endpoint;
	else
		endpoints.erase(entry);
	if(!status && flags & FLAG_CLOSING)
		status = XE_ECANCELED;
	while(conn){
		next = conn -> next;
		conn -> next = null;
		conn -> prev = null;
		conn -> resolved(endpoint, status);
		conn = next;
	}
}

int xe_net_ctx::poll(xe_connection& conn, int mode, int fd, int flags){
	epoll_event event;

	event.events = flags;
	event.data.ptr = &conn;

	if(epoll_ctl(pollfd, mode, fd, &event) < 0)
		return xe_syserror(errno);
	return 0;
}

void xe_net_ctx::count(){
	if(!conn_count)
		xe_net_poll(*this);
	conn_count++;
}

void xe_net_ctx::uncount(){
	conn_count--;
}

void xe_net_ctx::add(xe_connection& conn){
	xe_assert(conn.next == null && conn.prev == null);

	if(connections)
		connections -> prev = &conn;
	conn.next = connections;
	connections = &conn;
}

void xe_net_ctx::remove(xe_connection& conn){
	if(flags & FLAG_CLOSING)
		return;
	xe_assert((conn.prev == null) == (&conn == connections));

	if(conn.next)
		conn.next -> prev = conn.prev;
	if(conn.prev)
		conn.prev -> next = conn.next;
	else
		connections = conn.next;
}

xe_net_ctx::xe_net_ctx(xe_loop& loop): loop(loop), resolver(loop){
	ssl_ctx = null;

	for(uint i = 0; i < XE_PROTOCOL_LAST; i++)
		protocols[i] = null;
	connections = null;

	flags = 0;
	conn_count = 0;
	pollfd = -1;
	handle = -1;
}

xe_net_ctx::~xe_net_ctx(){

}

int xe_net_ctx::init(xe_net_shared& shared){
	int err = XE_ENOMEM;
	int epfd = -1;

	for(uint i = 0; i < XE_PROTOCOL_LAST; i++){
		protocols[i] = allocate_protocol(*this, i);

		if(!protocols[i])
			goto fail;
	}

	epfd = epoll_create(MAX_EVENTS);

	if(epfd < 0){
		err = xe_syserror(errno);

		goto fail;
	}

	if((err = resolver.init(shared.resolve)))
		goto fail;
	ssl_ctx = &shared.ssl;
	pollfd = epfd;

	return 0;

	fail:

	for(uint i = 0; i < XE_PROTOCOL_LAST; i++){
		if(!protocols[i])
			break;
		xe_delete(protocols[i]);
	}

	if(epfd != -1)
		close(epfd);
	return err;
}

void xe_net_ctx::destroy(){
	xe_connection* conn;
	xe_connection* next;

	flags |= FLAG_CLOSING;
	conn = connections;
	connections = null;

	while(conn){
		next = conn -> next;
		conn -> prev = null;
		conn -> next = null;
		conn -> close(XE_ECANCELED);
		conn = next;
	}

	if(flags & FLAG_ACTIVE)
		stop();
	if(handle != -1){
		loop.modify_handle(handle, null, null, pollfd, 0);
		handle = -1;
	}else if(pollfd != -1){
		close(pollfd);
	}

	resolver.close();

	for(auto& endpoint : endpoints){
		endpoint.first.free();
		endpoint.second.endpoint.free();
	}

	for(uint i = 0; i < XE_PROTOCOL_LAST; i++)
		xe_delete(protocols[i]);
}

int xe_net_ctx::start(){
	if(flags & FLAG_ACTIVE)
		return 0;
	if(loop.remain() < 2)
		return XE_ETOOMANYHANDLES;
	loop.reserve(2);

	int err = resolver.start();

	if(err)
		loop.release(2);
	else
		flags |= FLAG_ACTIVE;
	return err;
}

void xe_net_ctx::stop(){
	if(!(flags & FLAG_ACTIVE))
		return;
	resolver.stop();

	if(handle == -1){
		loop.release(2);

		return;
	}

	int ret;

	loop.release(1);
	ret = loop.poll_cancel(handle, null, null, 0, 0, XE_HANDLE_DISCARD);

	xe_assert(ret >= 0);
}

int xe_net_ctx::open(xe_request& request, xe_string _url){
	xe_string url;

	if(!url.copy(_url))
		return XE_ENOMEM;
	xe_protocol_data* data = request.data;
	xe_url parser(url);

	int err = parser.parse();

	if(err)
		return err;
	xe_string scheme = parser.scheme();

	for(uint i = 0; i < XE_PROTOCOL_LAST; i++){
		if(protocols[i] -> matches(scheme)){
			err = protocols[i] -> open(request, parser);

			if(err)
				return err;
			if(request.data != data)
				xe_delete(data);
			return 0;
		}
	}

	return XE_ENOSYS;
}

int xe_net_ctx::start(xe_request& request){
	if(request.state != XE_REQUEST_STATE_COMPLETE && request.state != XE_REQUEST_STATE_IDLE)
		return XE_EINVAL;
	xe_protocol* protocol = protocols[request.data -> id()];

	return protocol -> start(request);
}

xe_loop& xe_net_ctx::get_loop(){
	return loop;
}

xe_ssl_ctx& xe_net_ctx::get_ssl_ctx(){
	return *ssl_ctx;
}

void xe_net_ctx::io(xe_handle& handle, int result){
	if(!handle.user_data){
		close(handle.u1);

		return;
	}

	xe_net_ctx& net = *(xe_net_ctx*)handle.user_data;

	net.handle = -1;

	if(!(net.flags & FLAG_ACTIVE))
		return;
	xe_assert(net.loop.reserve(1));

	epoll_event events[MAX_EVENTS];
	ulong buf;

	int n = epoll_wait(net.pollfd, events, MAX_EVENTS, 0);

	if(n < 0){
		xe_assert(errno == EINTR);

		return;
	}

	for(int i = 0; i < n; i++){
		xe_connection* conn = (xe_connection*)events[i].data.ptr;

		conn -> io(events[i].events);
	}

	if(net.conn_count)
		xe_net_poll(net);
}

int xe_net::xe_net_init(){
	return xe_ssl_init();
}

void xe_net::xe_net_cleanup(){
	xe_ssl_cleanup();
}