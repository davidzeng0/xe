#include <ares.h>
#include <sys/epoll.h>
#include <unistd.h>
#include "ctx.h"
#include "url.h"
#include "conn.h"
#include "request.h"
#include "protocol.h"
#include "proto/http.h"
#include "proto/ws.h"
#include "proto/file.h"
#include "xutil/mem.h"
#include "xe/error.h"
#include "xutil/log.h"

using namespace xurl;

enum{
	MAX_EVENTS = 256
};

int xurl_shared::init(xe_ssl_ctx& ssl){
	xe_return_error(resolve.init());

	ssl_ctx = &ssl;

	return 0;
}

xe_ssl_ctx& xurl_shared::ssl(){
	return *ssl_ctx;
}

void xurl_shared::free(){
	resolve.close();
}

void xurl_ctx::poll(){
	if(handle != -1 || !active)
		return;
	loop_ -> release(1);
	handle = loop_ -> poll(pollfd, EPOLLIN, this, null, 0, 0, XURL_CONNECTION);

	xe_assert(handle >= 0);
}

static xe_protocol* allocate_protocol(xurl_ctx& ctx, int id){
	switch(id){
		case XE_PROTOCOL_FILE:
			return xe_file_new(ctx);
		case XE_PROTOCOL_HTTP:
			return xe_http_new(ctx);
		case XE_PROTOCOL_WEBSOCKET:
			return xe_websocket_new(ctx);
		default:
			return null;
	}
}

int xurl_ctx::resolve(xe_connection& conn, const xe_string_view& host, xe_endpoint*& ep){
	auto entry = endpoints.find((xe_string&)host);

	if(entry != endpoints.end()){
		if(entry -> second.pending){
			conn.next = entry -> second.pending;

			if(conn.next)
				conn.next -> prev = &conn;
		}else{
			ep = &entry -> second.endpoint;

			return 0;
		}
	}else{
		xe_string host_copy;
		int err;

		if(!host_copy.copy(host))
			return XE_ENOMEM;
		entry = endpoints.insert(std::move(host_copy));

		if(entry == endpoints.end())
			return XE_ENOMEM;
		entry -> second.pending = null;
		conn.next = null;
		err = resolver.resolve(entry -> first, entry -> second.endpoint);

		if(err != XE_EINPROGRESS){
			if(err)
				endpoints.erase(entry);
			else
				ep = &entry -> second.endpoint;
			return err;
		}
	}

	conn.prev = null;
	entry -> second.pending = &conn;

	return XE_EINPROGRESS;
}

void xurl_ctx::resolved(const xe_string_view& host, xe_endpoint&& endpoint, int status){
	xe_connection* conn;
	xe_connection* next;

	int err;
	auto entry = endpoints.find((xe_string&)host);

	xe_assert(entry != endpoints.end());

	conn = entry -> second.pending;
	entry -> second.pending = null;

	if(!status)
		entry -> second.endpoint = std::move(endpoint);
	else
		endpoints.erase(entry);
	if(!status && closing)
		status = XE_ECANCELED;
	while(conn){
		next = conn -> next;
		conn -> next = null;
		conn -> prev = null;
		conn -> start_connect(entry -> second.endpoint, status);
		conn = next;
	}
}

int xurl_ctx::poll(xe_connection& conn, int mode, int fd, int flags){
	epoll_event event;

	event.events = flags;
	event.data.ptr = &conn;

	if(epoll_ctl(pollfd, mode, fd, &event) < 0)
		return xe_errno();
	return 0;
}

void xurl_ctx::count(xe_connection& conn){
	/* refcount */
#ifdef XE_DEBUG
	xe_assert(!conn.refcounted);

	conn.refcounted = true;
#endif
	if(!conn_count)
		poll();
	xe_assert(conn_count < SIZE_MAX);

	conn_count++;
}

void xurl_ctx::uncount(xe_connection& conn){
	/* refcount */
#ifdef XE_DEBUG
	xe_assert(conn.refcounted);

	conn.refcounted = false;
#endif
	conn_count--;
}

void xurl_ctx::add(xe_connection& conn){
	xe_assert(conn.next == null && conn.prev == null);

	if(connections)
		connections -> prev = &conn;
	conn.next = connections;
	connections = &conn;
}

void xurl_ctx::remove(xe_connection& conn){
	if(closing)
		return;
	xe_assert((conn.prev == null) == (&conn == connections));

	if(conn.next)
		conn.next -> prev = conn.prev;
	if(conn.prev)
		conn.prev -> next = conn.next;
	else
		connections = conn.next;
}

xurl_ctx::xurl_ctx(){
	shared = null;

	for(uint i = 0; i < XE_PROTOCOL_LAST; i++)
		protocols[i] = null;
	connections = null;
	active = false;
	closing = false;
	flags = 0;
	conn_count = 0;
}

int xurl_ctx::init(xe_loop& loop, xurl_shared& shared_){
	int err = XE_ENOMEM;
	int epfd = -1;

	loop_ = &loop;
	shared = &shared_;
	pollfd = -1;
	handle = -1;
	endpoints.init();

	for(uint i = 0; i < XE_PROTOCOL_LAST; i++){
		protocols[i] = allocate_protocol(*this, i);

		if(!protocols[i])
			goto fail;
	}

	epfd = epoll_create(MAX_EVENTS);

	if(epfd < 0){
		err = xe_errno();

		goto fail;
	}

	if((err = resolver.init(*this, shared_.resolve)))
		goto fail;
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

void xurl_ctx::destroy(){
	xe_connection* conn;
	xe_connection* next;

	closing = true;
	conn = connections;
	connections = null;

	while(conn){
		next = conn -> next;
		conn -> prev = null;
		conn -> next = null;
		conn -> close(XE_ECANCELED);
		conn = next;
	}

	if(active)
		stop();
	if(handle != -1){
		loop_ -> modify_handle(handle, null, null, pollfd, 0);
		handle = -1;
	}else if(pollfd != -1){
		close(pollfd);
	}

	resolver.close();
	endpoints.free();

	for(uint i = 0; i < XE_PROTOCOL_LAST; i++)
		xe_delete(protocols[i]);
}

int xurl_ctx::start(){
	if(active)
		return 0;
	if(loop_ -> remain() < 2)
		return XE_ETOOMANYHANDLES;
	loop_ -> reserve(2);

	int err = resolver.start();

	if(err)
		loop_ -> release(2);
	else
		active = true;
	return err;
}

void xurl_ctx::stop(){
	if(!active)
		return;
	resolver.stop();

	if(handle == -1){
		loop_ -> release(2);

		return;
	}

	int ret;

	loop_ -> release(1);
	ret = loop_ -> poll_cancel(handle, null, null, 0, 0, XE_LOOP_HANDLE_DISCARD);

	xe_assert(ret >= 0);
}

int xurl_ctx::open(xe_request& request, const xe_string_view& url_){
	xe_string url;

	if(!url.copy(url_))
		return XE_ENOMEM;
	xe_protocol_specific* data = request.data;
	xe_url parser(std::move(url));

	xe_return_error(parser.parse());

	for(uint i = 0; i < XE_PROTOCOL_LAST; i++){
		if(protocols[i] -> matches(parser.scheme())){
			xe_return_error(protocols[i] -> open((xe_request_internal&)request, std::move(parser)));

			if(request.data != data)
				xe_delete(data);
			return 0;
		}
	}

	return XE_ENOSYS;
}

int xurl_ctx::start(xe_request& request){
	if(request.state != XE_REQUEST_STATE_COMPLETE && request.state != XE_REQUEST_STATE_IDLE)
		return XE_EINVAL;
	xe_protocol* protocol = protocols[request.data -> id()];

	return protocol -> start((xe_request_internal&)request);
}

int xurl_ctx::transferctl(xe_request& request, uint flags){
	if(request.state == XE_REQUEST_STATE_COMPLETE || request.state == XE_REQUEST_STATE_IDLE)
		return XE_EINVAL;
	xe_protocol* protocol = protocols[request.data -> id()];

	return protocol -> transferctl((xe_request_internal&)request, flags);
}

int xurl_ctx::end(xe_request& request){
	if(request.state == XE_REQUEST_STATE_COMPLETE || request.state == XE_REQUEST_STATE_IDLE)
		return XE_EINVAL;
	xe_protocol* protocol = protocols[request.data -> id()];

	protocol -> end((xe_request_internal&)request);

	return 0;
}

xe_loop& xurl_ctx::loop(){
	return *loop_;
}

xe_ssl_ctx& xurl_ctx::ssl(){
	return shared -> ssl();
}

void xurl_ctx::io(xe_loop_handle& handle, int result){
	if(!handle.user_data){
		close(handle.u1);

		return;
	}

	xurl_ctx& ctx = *(xurl_ctx*)handle.user_data;
	epoll_event events[MAX_EVENTS];
	xe_connection* conn;

	ctx.handle = -1;

	if(!ctx.active)
		return;
	xe_assert(ctx.loop_ -> reserve(1));

	int n = epoll_wait(ctx.pollfd, events, MAX_EVENTS, 0);

	if(n < 0){
		xe_assert(xe_errno() == XE_EINTR);

		return;
	}

	xe_log_trace(&ctx, "processing %i events", n);

	for(int i = 0; i < n; i++){
		conn = (xe_connection*)events[i].data.ptr;
		conn -> io(events[i].events);
	}

	if(ctx.conn_count)
		ctx.poll();
}

size_t xurl_ctx::connection_count(){
	return conn_count;
}

xe_cstr xurl_ctx::class_name(){
	return "xurl_ctx";
}