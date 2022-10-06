#include <sys/epoll.h>
#include <sys/eventfd.h>
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
#include "xutil/log.h"
#include "xe/error.h"

using namespace xurl;

enum{
	MAX_EVENTS = 256
};

int xurl_shared::init(){
	int err;

	if((err = resolve_ctx_.init()))
		goto err_out;
	if((err = ssl_ctx_.init()))
		goto err_ssl;
	if((err = ssl_ctx_.load_default_verify_locations()))
		goto err_load;
	return 0;
err_load:
	ssl_ctx_.close();
err_ssl:
	resolve_ctx_.close();
err_out:
	return err;
}

const xe_resolve_ctx& xurl_shared::resolve_ctx(){
	return resolve_ctx_;
}

const xe_ssl_ctx& xurl_shared::ssl_ctx(){
	return ssl_ctx_;
}

void xurl_shared::close(){
	resolve_ctx_.close();
	ssl_ctx_.close();
	endpoints.clear();
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
		/* host resolution is in progress */
		if(entry -> second -> pending){
			conn.next = entry -> second -> pending;

			if(conn.next)
				conn.next -> prev = &conn;
		}else{
			ep = &entry -> second -> endpoint;

			return 0;
		}
	}else{
		xe_string host_copy;
		xe_unique_ptr<xe_resolve_entry> data;
		int err;

		data.own(xe_zalloc<xe_resolve_entry>());

		if(!data || !host_copy.copy(host))
			return XE_ENOMEM;
		entry = endpoints.insert(std::move(host_copy));

		if(entry == endpoints.end())
			return XE_ENOMEM;
		entry -> second = std::move(data);
		err = resolver.resolve(entry -> first, entry -> second -> endpoint, resolved, this);

		if(err != XE_EINPROGRESS){
			if(err)
				endpoints.erase(entry);
			else
				ep = &entry -> second -> endpoint;
			return err;
		}

		conn.next = null;
		active_resolves_++;
	}

	entry -> second -> pending = &conn;
	/* set prev to clear the link list head if cancelled */
	conn.prev = (xe_connection*)&*entry -> second;

	return XE_EINPROGRESS;
}

void xurl_ctx::resolved(xe_ptr data, const xe_string_view& host, xe_endpoint&& endpoint, int status){
	xurl_ctx& ctx = *(xurl_ctx*)data;
	xe_connection* conn;
	xe_connection* next;

	auto entry = ctx.endpoints.find((xe_string&)host);

	xe_assert(entry != ctx.endpoints.end());

	ctx.active_resolves_--;
	conn = entry -> second -> pending;
	entry -> second -> pending = null;

	if(!status && ctx.closing)
		status = XE_ECANCELED;
	if(!status)
		entry -> second -> endpoint = std::move(endpoint);
	else
		ctx.endpoints.erase(entry);
	while(conn){
		next = conn -> next;
		conn -> next = null;
		conn -> prev = null;

		if(status)
			conn -> close(status);
		else
			conn -> start_connect(entry -> second -> endpoint);
		conn = next;
	}
}

void xurl_ctx::add(xe_connection& conn){
	xe_assert(conn.next == null && conn.prev == null);

	if(connections)
		connections -> prev = &conn;
	conn.next = connections;
	connections = &conn;
}

void xurl_ctx::resolve_remove(xe_connection& conn){
	if(conn.next)
		conn.next -> prev = conn.prev;
	if(conn.prev)
		conn.prev -> next = conn.next;
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

void xurl_ctx::count(){
	active_connections_++;
}

void xurl_ctx::uncount(){
	active_connections_--;
}

int xurl_ctx::init(xe_loop& loop, xurl_shared& shared_){
	size_t i;
	int err;

	loop_ = &loop;
	shared = &shared_;
	err = XE_ENOMEM;

	for(i = 0; i < protocols.size(); i++){
		protocols[i] = allocate_protocol(*this, i);

		if(!protocols[i]) goto fail;
	}

	err = resolver.init(loop, shared_.resolve_ctx());

	if(err)
		goto fail;
	return 0;
fail:
	while(i--)
		xe_delete(protocols[i]);
	return err;
}

void xurl_ctx::close(){
	xe_connection* conn;
	xe_connection* next;

	conn = connections;
	connections = null;

	resolver.close();
	endpoints.clear();

	while(conn){
		next = conn -> next;
		conn -> prev = null;
		conn -> next = null;
		conn -> close(XE_ECANCELED);
		conn = next;
	}

	for(auto protocol : protocols)
		xe_delete(protocol);
}

int xurl_ctx::open(xe_request& request, const xe_string_view& url_){
	xe_string url;

	if(request.state() != XE_REQUEST_STATE_COMPLETE && request.state() != XE_REQUEST_STATE_IDLE)
		return XE_STATE;
	if(!url.copy(url_))
		return XE_ENOMEM;
	xe_protocol_specific* data = request.data;
	xe_url parser(std::move(url));

	xe_return_error(parser.parse());

	for(auto protocol : protocols){
		if(!protocol -> matches(parser.scheme()))
			continue;
		xe_return_error(protocol -> open((xe_request_internal&)request, std::move(parser)));

		if(request.data != data)
			xe_delete(data);
		return 0;
	}

	return XE_EPROTONOSUPPORT;
}

int xurl_ctx::start(xe_request& request){
	if(request.state() != XE_REQUEST_STATE_COMPLETE && request.state() != XE_REQUEST_STATE_IDLE)
		return XE_STATE;
	xe_protocol* protocol = protocols[request.data -> id()];

	return protocol -> start((xe_request_internal&)request);
}

int xurl_ctx::transferctl(xe_request& request, uint flags){
	if(request.state() == XE_REQUEST_STATE_COMPLETE || request.state() == XE_REQUEST_STATE_IDLE)
		return XE_STATE;
	xe_protocol* protocol = protocols[request.data -> id()];

	return protocol -> transferctl((xe_request_internal&)request, flags);
}

int xurl_ctx::end(xe_request& request){
	if(request.state() == XE_REQUEST_STATE_COMPLETE || request.state() == XE_REQUEST_STATE_IDLE)
		return XE_ENOENT;
	xe_protocol* protocol = protocols[request.data -> id()];

	protocol -> end((xe_request_internal&)request);

	return 0;
}

xe_cstr xurl_ctx::class_name(){
	return "xurl_ctx";
}