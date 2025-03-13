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
	DNS_EXPIRE = 60 * 1'000'000'000ul /* 60 seconds */
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

void xe_connection_ctx::resolved(const xe_shared_ref<xe_endpoint>& endpoint, xe_linked_list<xe_connection>& pending, int status){
	xe_connection* conn;

	while(pending){
		conn = &pending.front();
		pending.erase(*conn);
		list.append(*conn);

		if(status)
			conn -> close(status);
		else
			conn -> start_connect(endpoint);
	}
}

void xe_connection_ctx::close(){
	xe_connection* conn;
	auto cur = list.begin(),
		end = list.end();
	while(cur != end){
		conn = &*(cur++);
		conn -> close(XE_ECANCELED);
	}
}

bool xe_connection_ctx::closing(){
	return list || close_pending;
}

xe_loop& xe_connection_ctx::loop(){
	xurl_ctx& ctx = xe_containerof(*this, &xurl_ctx::connections);

	return ctx.loop();
}

int xe_connection_ctx::resolve(xe_connection& conn, const xe_string_view& host, xe_shared_ref<xe_endpoint>& ep){
	xurl_ctx& ctx = xe_containerof(*this, &xurl_ctx::connections);
	xe_linked_list<xe_connection>* list;
	int res;

	res = ctx.resolve(conn, host, ep, list);

	if(res == XE_EINPROGRESS){
		/* append to waiting list */
		list -> append(conn);
	}

	return res;
}

void xe_connection_ctx::add(xe_connection& conn){
	xe_assert(!conn.linked());

	list.append(conn);
}

void xe_connection_ctx::closing(xe_connection& conn){
	xe_assert(conn.linked());

	list.erase(conn);
	close_pending.append(conn);
}

void xe_connection_ctx::remove(xe_connection& conn){
	xurl_ctx& ctx = xe_containerof(*this, &xurl_ctx::connections);

	xe_assert(conn.linked());

	conn.erase();

	if(ctx.closing) ctx.check_close();
}

void xurl_ctx::resolved(xe_ptr data, const xe_string_view& host, xe_endpoint&& endpoint, int status){
	xurl_ctx& ctx = *(xurl_ctx*)data;
	auto it = ctx.endpoints.find((const xe_string&)host);
	xe_resolve_entry& entry = *it -> second;

	xe_assert(it != ctx.endpoints.end());

	ctx.active_resolves_--;

	if(!status && ctx.closing)
		status = XE_ECANCELED;
	if(!status){
		*entry.endpoint = std::move(endpoint);

		ctx.resolve_success(entry);
	}

	ctx.connections.resolved(entry.endpoint, entry.pending, status);

	if(status) ctx.endpoints.erase(it);
}

void xurl_ctx::close_cb(xe_resolve& resolve){
	xurl_ctx& ctx = xe_containerof(resolve, &xurl_ctx::resolver);

	ctx.resolver_closing = false;
	ctx.check_close();
}

void xurl_ctx::expire_cb(xe_loop& loop, xe_timer& timer){
	xurl_ctx& ctx = xe_containerof(timer, &xurl_ctx::expire_timer);

	ctx.purge_expired();

	if(ctx.expire) ctx.start_expire_timer();
}

xe_resolve_entry::xe_resolve_entry(xe_shared_ref<xe_endpoint>&& ep): endpoint(std::move(ep)){}

int xurl_ctx::alloc_entry(const xe_string_view& host, xe_map<xe_string, xe_unique_ptr<xe_resolve_entry>>::iterator& it){
	xe_shared_data<xe_endpoint>* shared = xe_znew<xe_shared_data<xe_endpoint>>();
	xe_string host_copy;

	if(!shared)
		return XE_ENOMEM;
	xe_shared_ref<xe_endpoint> ref(*shared);
	xe_unique_ptr<xe_resolve_entry> data(xe_znew<xe_resolve_entry>(std::move(ref)));

	if(!data || !host_copy.copy(host))
		return XE_ENOMEM;
	it = endpoints.insert(std::move(host_copy));

	if(it == endpoints.end())
		return XE_ENOMEM;
	it -> second = std::move(data);
	it -> second -> key = it -> first;

	return 0;
}

void xurl_ctx::start_expire_timer(){
	xe_resolve_entry& entry = expire.front();

	xe_assertz(loop_ -> timer_ms(expire_timer, entry.time, 0, XE_TIMER_PASSIVE | XE_TIMER_ABS));
}

void xurl_ctx::resolve_success(xe_resolve_entry& entry){
	entry.time = xe_time_ns() + DNS_EXPIRE;
	expire.append(entry);

	if(!expire_timer.active()) start_expire_timer();
}

void xurl_ctx::purge_expired(){
	ulong now = xe_time_ns();

	while(expire){
		xe_resolve_entry& entry = expire.front();

		if(now < entry.time)
			break;
		expire.erase(entry);
		endpoints.erase((const xe_string&)entry.key);
	}

	endpoints.trim();
}

int xurl_ctx::resolve(xe_connection& conn, const xe_string_view& host, xe_shared_ref<xe_endpoint>& ep, xe_linked_list<xe_connection>*& queue){
	purge_expired();

	auto it = endpoints.find((const xe_string&)host);
	int err;

	if(it != endpoints.end()){
		/* host resolution already started */
		if(!it -> second -> in_progress){
			/* host resolution already completed */
			ep = it -> second -> endpoint;

			return 0;
		}
	}else{
		xe_return_error(alloc_entry(host, it));

		err = resolver.resolve(it -> first, *it -> second -> endpoint, resolved, this);

		if(err != XE_EINPROGRESS){
			if(err)
				endpoints.erase(it);
			else{
				ep = it -> second -> endpoint;

				resolve_success(*it -> second);
			}

			return err;
		}

		active_resolves_++;
	}

	queue = &it -> second -> pending;

	return XE_EINPROGRESS;
}

void xurl_ctx::check_close(){
	if(resolver_closing || connections.closing())
		return;
	closing = false;

	if(close_callback) close_callback(*this);
}

static xe_protocol* allocate_protocol(xurl_ctx& ctx, xe_protocol_id id){
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

int xurl_ctx::init(xe_loop& loop, xurl_shared& shared_){
	int err;

	loop_ = &loop;
	shared = &shared_;
	err = 0;

	for(size_t i = 0; i < protocols.size(); i++){
		protocols[i] = allocate_protocol(*this, (xe_protocol_id)i);

		if(!protocols[i]){
			err = XE_ENOMEM;

			break;
		}
	}

	if(!err)
		err = resolver.init(loop, shared_.resolve_ctx());
	if(err)
		protocols.clear();
	return err;
}

int xurl_ctx::close(){
	int res;

	if(closing)
		return XE_EALREADY;
	closing = true;
	res = resolver.close();
	endpoints.clear();

	if(res)
		resolver_closing = true;
	connections.close();

	for(auto& protocol : protocols)
		protocol.clear();
	if(!resolver_closing && !connections.closing())
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

	for(auto& protocol : protocols){
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