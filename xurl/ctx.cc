#include "ctx.h"
#include "url.h"
#include "ssl.h"
#include "conn.h"
#include "request.h"
#include "protocol.h"
#include "proto/http.h"
#include "proto/ws.h"
#include "proto/file.h"
#include "xutil/mem.h"
#include "xutil/log.h"
#include "xe/error.h"
#include "xe/clock.h"

using namespace xurl;

enum{
	DNS_EXPIRE = 60 * 1000 /* 60 seconds */
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
		/* host resolution already started */
		if(!entry -> second -> in_progress){
			/* host resolution already completed */
			ep = &entry -> second -> endpoint;

			return 0;
		}
	}else{
		xe_string host_copy;
		xe_unique_ptr<xe_resolve_entry> data;
		int err;

		data.own(xe_znew<xe_resolve_entry>());

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

		active_resolves_++;
	}

	/* append to waiting list */
	entry -> second -> append(conn.node);

	return XE_EINPROGRESS;
}

void xurl_ctx::close_cb(xe_resolve& resolve){
	xurl_ctx& ctx = xe_containerof(resolve, &xurl_ctx::resolver);

	ctx.resolver_closing = false;
	ctx.check_close();
}

void xurl_ctx::resolved(xe_ptr data, const xe_string_view& host, xe_endpoint&& endpoint, int status){
	xurl_ctx& ctx = *(xurl_ctx*)data;
	auto entry = ctx.endpoints.find((xe_string&)host);
	xe_resolve_entry& list = *entry -> second;
	xe_connection* conn;

	xe_assert(entry != ctx.endpoints.end());

	ctx.active_resolves_--;

	if(!status && ctx.closing)
		status = XE_ECANCELED;
	if(!status)
		list.endpoint = std::move(endpoint);
	while(list){
		conn = &xe_containerof(list.head(), &xe_connection::node);
		list.erase(conn -> node);
		ctx.connections.append(conn -> node);

		if(status)
			conn -> close(status);
		else
			conn -> start_connect(list.endpoint);
	}

	if(status) ctx.endpoints.erase(entry);
}

void xurl_ctx::add(xe_connection& conn){
	xe_assert(!conn.node.in_list());

	connections.append(conn.node);
}

void xurl_ctx::remove(xe_connection& conn){
	xe_assert(conn.node.in_list());

	connections.erase(conn.node);

	if(closing) check_close();
}

void xurl_ctx::check_close(){
	if(resolver_closing || connections)
		return;
	closing = false;

	if(close_callback) close_callback(*this);
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

int xurl_ctx::close(){
	xe_connection* conn;
	int res;

	if(closing)
		return XE_EALREADY;
	closing = true;
	res = resolver.close();
	endpoints.clear();

	if(res)
		resolver_closing = true;
	auto cur = connections.begin(),
		end = connections.end();
	while(cur != end){
		conn = &xe_containerof(*(cur++), &xe_connection::node);
		conn -> close(XE_ECANCELED);
	}

	for(auto protocol : protocols)
		xe_delete(protocol);
	if(!resolver_closing && !connections)
		closing = false;
	else if(!res)
		res = XE_EINPROGRESS;
	return res;
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

	return XE_ENOPROTOOPT;
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