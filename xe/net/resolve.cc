#include <ares.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "net.h"
#include "resolve.h"
#include "../mem.h"
#include "../error.h"
#include "../common.h"
#include "../log.h"

using namespace xe_net;

enum xe_resolver_constants{
	MAX_EVENTS = 256,
	TIMEOUT = 10000
};

enum xe_resolver_flags{
	FLAG_NONE = 0x0,
	FLAG_ACTIVE = 0x1
};

struct xe_resolve_ctx::data{
	ares_options options;

	int optmask;
};

struct xe_resolve_query{
	xe_string host;
	xe_endpoint endpoint;

	xe_net_ctx* net;

#ifdef XE_DEBUG
	ulong start;
	xe_resolve* resolve;
#endif

	int error;
	bool done;
	bool returned;
};

static const ares_addrinfo_hints hints = {
	ARES_AI_NOSORT,
	AF_UNSPEC,
	0,
	0
};

int xe_resolve_ctx::init(){
	ares_channel channel;
	data* data;

	int ret;

	data = xe_zalloc<struct data>();

	if(!data)
		return XE_ENOMEM;
	data -> options.timeout = TIMEOUT;
	data -> options.tries = 3;
	data -> options.lookups = (xe_pchar)"b";

	ret = ares_init_options(&channel, &data -> options, ARES_OPT_TIMEOUTMS | ARES_OPT_LOOKUPS | ARES_OPT_TRIES);

	if(ret != ARES_SUCCESS){
		xe_dealloc(data);

		if(ret == ARES_ENOMEM)
			return XE_ENOMEM;
		return XE_ERESOLVER;
	}

	ret = ares_save_options(channel, &data -> options, &data -> optmask);

	ares_destroy(channel);

	if(ret){
		xe_dealloc(data);

		return XE_ENOMEM;
	}

	priv = data;

	return 0;
}

void xe_resolve_ctx::close(){
	if(!priv)
		return;
	ares_destroy_options(&priv -> options);
	xe_dealloc(priv);
}

static void xe_resolve_poll(xe_resolve& resolve){
	if(resolve.handle != -1 || !(resolve.flags & FLAG_ACTIVE))
		return;
	resolve.loop.release(1);
	resolve.handle = resolve.loop.poll(resolve.pollfd, EPOLLIN, &resolve, null, 0, 0, XE_NET_RESOLVER);

	xe_assert(resolve.handle >= 0);
}

static void xe_resolve_sockstate(xe_ptr data, int fd, int read, int write){
	xe_resolve& resolve = *(xe_resolve*)data;
	epoll_event event;

	if(!(read || write)){
		resolve.count--;

		epoll_ctl(resolve.pollfd, EPOLL_CTL_DEL, fd, null);

		return;
	}

	event.events = 0;
	event.data.fd = fd;

	if(read)
		event.events |= EPOLLIN;
	if(write)
		event.events |= EPOLLOUT;
	if(epoll_ctl(resolve.pollfd, EPOLL_CTL_MOD, fd, &event))
		shutdown(fd, SHUT_RDWR);
}

static int xe_resolve_sockcreate(int fd, int type, xe_ptr data){
	xe_resolve& resolve = *(xe_resolve*)data;
	epoll_event event;

	event.events = 0;
	event.data.u64 = fd;

	int err = epoll_ctl(resolve.pollfd, EPOLL_CTL_ADD, fd, &event);

	if(err)
		return err;
	if(!resolve.count)
		xe_resolve_poll(resolve);
	xe_assert(resolve.count < UINT_MAX);

	resolve.count++;

	return 0;
}

static int xe_ares_error(int status){
	switch(status){
		case ARES_SUCCESS:
			return XE_NONE;

			break;
		case ARES_EBADNAME:
			return XE_RESOLVER_BADNAME;

			break;
		case ARES_ENODATA:
		case ARES_ENOTFOUND:
			return XE_RESOLVER_UNKNOWN_HOST;

			break;
		case ARES_ENOMEM:
			return XE_ENOMEM;

			break;
		case ARES_ECANCELLED:
		case ARES_EDESTRUCTION:
			return XE_ECANCELED;

			break;
		case ARES_ETIMEOUT:
			return XE_RESOLVER_TIMEOUT;

			break;
		case ARES_EFORMERR:
			return XE_RESOLVER_CLIENT_FAIL;

			break;
		case ARES_ESERVFAIL:
		case ARES_EREFUSED:
		case ARES_EBADRESP:
			return XE_RESOLVER_SERVER_FAIL;

			break;
		case ARES_ECONNREFUSED:
			return XE_RESOLVER_CONNREFUSED;

			break;
		case ARES_ENOTIMP:
			return XE_ENOSYS;
		default:
			return XE_ERESOLVER;

			break;
	}
}

static int xe_resolve_to_addresslist(xe_endpoint& list, ares_addrinfo* result){
	size_t inet_len = 0, inet6_len = 0;
	ares_addrinfo_node* node;

	node = result -> nodes;

	while(node){
		if(node -> ai_family == AF_INET)
			inet_len++;
		else if(node -> ai_family == AF_INET6)
			inet6_len++;
		else
			xe_notreached;
		node = node -> ai_next;
	}

	if(inet_len && !list.inet.resize(inet_len))
		return XE_ENOMEM;
	if(inet6_len && !list.inet6.resize(inet6_len)){
		list.inet.free();

		return XE_ENOMEM;
	}

	inet_len = 0;
	inet6_len = 0;
	node = result -> nodes;

	while(node){
		if(node -> ai_family == AF_INET)
			xe_tmemcpy(&list.inet[inet_len++], &((sockaddr_in*)node -> ai_addr) -> sin_addr);
		else if(node -> ai_family == AF_INET6)
			xe_tmemcpy(&list.inet6[inet6_len++], &((sockaddr_in6*)node -> ai_addr) -> sin6_addr);
		else
			xe_notreached;
		node = node -> ai_next;
	}

	return 0;
}

static void xe_resolve_callback(xe_ptr data, int status, int timeouts, ares_addrinfo* result){
	xe_resolve_query& query = *(xe_resolve_query*)data;

	status = xe_ares_error(status);

	if(!status){
		status = xe_resolve_to_addresslist(query.endpoint, result);

		ares_freeaddrinfo(result);
	}
#ifdef XE_DEBUG
	if(status == XE_ECANCELED)
		goto end;
	xe_log_debug("xe_resolve", query.resolve, "resolved %s in %f ms, status: %s", query.host.c_str(), (xe_time_ns() - query.start) / 1e6, xe_strerror(status));

	char ip[INET6_ADDRSTRLEN];

	for(auto& addr : query.endpoint.inet){
		inet_ntop(AF_INET, &addr, ip, sizeof(ip));
		xe_log_trace("xe_resolve", query.resolve, "    %s", ip);
	}

	for(auto& addr : query.endpoint.inet6){
		inet_ntop(AF_INET6, &addr, ip, sizeof(ip));
		xe_log_trace("xe_resolve", query.resolve, "    %s", ip);
	}

	end:
#endif
	if(!query.returned){
		query.done = true;
		query.error = status;
	}else{
		query.net -> resolved(query.host, query.endpoint, status);

		xe_dealloc(&query);
	}
}

static int xe_resolve_ip(xe_string& host, xe_endpoint& endpoint){
	in_addr addr;
	in6_addr addr6;

	if(host == "localhost"){
		xe_asserteq(inet_pton(AF_INET, "127.0.0.1", &addr), 1);
		xe_asserteq(inet_pton(AF_INET6, "::1", &addr6), 1);

		if(!endpoint.inet.copy(&addr, 1))
			return XE_ENOMEM;
		if(!endpoint.inet6.copy(&addr6, 1)){
			endpoint.inet.free();

			return XE_ENOMEM;
		}

		return 0;
	}else{
		if(inet_pton(AF_INET, host.c_str(), &addr) == 1){
			if(!endpoint.inet.copy(&addr, 1))
				return XE_ENOMEM;
			return 0;
		}

		if(inet_pton(AF_INET6, host.c_str(), &addr6) == 1){
			if(!endpoint.inet6.copy(&addr6, 1))
				return XE_ENOMEM;
			return 0;
		}

		return XE_RESOLVER_UNKNOWN_HOST;
	}
}

xe_resolve::xe_resolve(xe_loop& loop): loop(loop){
	resolver = null;
	pollfd = -1;
	tfd = -1;
	count = 0;
	flags = 0;
	handle = -1;
}

int xe_resolve::init(xe_resolve_ctx& ctx){
	ares_channel channel;
	ares_options options;
	itimerspec it;
	epoll_event event;

	int pollfd, tfd;
	int ret;

	pollfd = -1;
	tfd = -1;
	channel = null;

	xe_tmemcpy(&options, &ctx.priv -> options);

	options.sock_state_cb = xe_resolve_sockstate;
	options.sock_state_cb_data = this;

	ret = ares_init_options(&channel, &options, ctx.priv -> optmask | ARES_OPT_SOCK_STATE_CB);

	if(ret != ARES_SUCCESS){
		if(ret == ARES_ENOMEM)
			return XE_ENOMEM;
		return XE_ERESOLVER;
	}

	pollfd = epoll_create(MAX_EVENTS);

	if(pollfd < 0)
		goto err;
	tfd = timerfd_create(CLOCK_MONOTONIC, 0);

	if(tfd < 0)
		goto err;
	it.it_value.tv_sec = TIMEOUT / 1000;
	it.it_value.tv_nsec = (TIMEOUT % 1000) * ((ulong)1e6);
	it.it_interval.tv_sec = it.it_value.tv_sec;
	it.it_interval.tv_nsec = it.it_value.tv_nsec;

	if(timerfd_settime(tfd, 0, &it, null) < 0)
		goto err;
	event.data.u64 = tfd;
	event.events = EPOLLIN;

	if(epoll_ctl(pollfd, EPOLL_CTL_ADD, tfd, &event) < 0)
		goto err;
	ares_set_socket_callback(channel, xe_resolve_sockcreate, this);

	this -> resolver = channel;
	this -> pollfd = pollfd;
	this -> tfd = tfd;

	return 0;

	err:

	if(tfd != -1)
		::close(tfd);
	if(pollfd != -1)
		::close(pollfd);
	if(channel)
		ares_destroy(channel);
	return xe_syserror(errno);
}

void xe_resolve::close(){
	if(flags & FLAG_ACTIVE)
		stop();
	if(resolver)
		ares_destroy((ares_channel)resolver);
	if(handle != -1){
		loop.modify_handle(handle, null, null, tfd, pollfd);
		handle = -1;

		return;
	}

	if(tfd != -1)
		::close(tfd);
	if(pollfd != -1)
		::close(pollfd);
}

int xe_resolve::start(){
	if(flags & FLAG_ACTIVE)
		return XE_EALREADY;
	if(handle != -1)
		return XE_EINVAL;
	if(loop.remain() < 2)
		return XE_ETOOMANYHANDLES;
	xe_assert(loop.reserve(2));

	flags |= FLAG_ACTIVE;

	return 0;
}

void xe_resolve::stop(){
	if(!(flags & FLAG_ACTIVE))
		return;
	flags &= ~FLAG_ACTIVE;

	if(handle == -1){
		loop.release(2);

		return;
	}

	int ret;

	loop.release(1);
	ret = loop.poll_cancel(handle, null, null, 0, 0, XE_HANDLE_DISCARD);

	xe_assert(ret >= 0);
}

int xe_resolve::resolve(xe_net_ctx& net, xe_string host, xe_endpoint& endpoint){
	xe_assertm(host.c_str()[host.length()] == 0, "host is not null terminated");

	int err = xe_resolve_ip(host, endpoint);

	if(err != XE_RESOLVER_UNKNOWN_HOST){
		xe_log_trace("xe_resolve", this, "ipresolve %s, status: %s", host.c_str(), xe_strerror(err));

		return err;
	}

	xe_resolve_query* query = xe_new<xe_resolve_query>();

	if(!query)
		return XE_ENOMEM;
	xe_log_trace("xe_resolve", this, "resolving %s", host.c_str());

	query -> host = host;
	query -> net = &net;
#ifdef XE_DEBUG
	query -> start = xe_time_ns();
	query -> resolve = this;
#endif
	ares_getaddrinfo((ares_channel)resolver, query -> host.c_str(), null, &hints, xe_resolve_callback, query);

	if(query -> done){
		err = query -> error;

		if(!err)
			endpoint = query -> endpoint;
		xe_dealloc(query);

		return err;
	}

	query -> returned = true;

	return XE_EINPROGRESS;
}

void xe_resolve::io(xe_handle& handle, int result){
	if(!handle.user_data){
		::close(handle.u1);
		::close(handle.u2);

		return;
	}

	xe_resolve& resolve = *(xe_resolve*)handle.user_data;

	resolve.handle = -1;

	if(!(resolve.flags & FLAG_ACTIVE))
		return;
	xe_assert(resolve.loop.reserve(1));

	epoll_event events[MAX_EVENTS];
	ulong buf;

	int n;
	int rfd, wfd;

	n = epoll_wait(resolve.pollfd, events, MAX_EVENTS, 0);

	if(n < 0){
		xe_assert(errno == EINTR);

		return;
	}

	for(int i = 0; i < n; i++){
		if(events[i].data.fd == resolve.tfd){
			rfd = ARES_SOCKET_BAD;
			wfd = ARES_SOCKET_BAD;

			xe_asserteq(read(resolve.tfd, &buf, sizeof(buf)), sizeof(buf));
		}else{
			rfd = events[i].events & (EPOLLIN | EPOLLERR | EPOLLRDHUP) ? events[i].data.fd : ARES_SOCKET_BAD;
			wfd = events[i].events & (EPOLLOUT | EPOLLERR | EPOLLRDHUP) ? events[i].data.fd : ARES_SOCKET_BAD;
		}

		ares_process_fd((ares_channel)resolve.resolver, rfd, wfd);
	}

	if(resolve.count)
		xe_resolve_poll(resolve);
}