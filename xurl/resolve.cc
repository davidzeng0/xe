#include <ares.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "xutil/overflow.h"
#include "xutil/mem.h"
#include "xe/error.h"
#include "xutil/xutil.h"
#include "xutil/log.h"
#include "xutil/clock.h"
#include "resolve.h"
#include "ctx.h"
#include "xutil/inet.h"

using namespace xurl;

enum{
	MAX_EVENTS = 256,
	TIMEOUT = 10000
};

struct xe_resolve_ctx::xe_resolve_ctx_data{
	ares_options options;

	int optmask;
};

struct xe_resolve_query{
	xe_string_view host;
	xe_endpoint endpoint;
	xe_resolve* resolve;
#ifdef XE_DEBUG
	ulong start;
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
	xe_resolve_ctx_data* data;

	int ret;

	data = xe_zalloc<xe_resolve_ctx_data>();

	if(!data)
		return XE_ENOMEM;
	data -> options.timeout = TIMEOUT;
	data -> options.tries = 3;
	data -> options.lookups = (xe_pchar)"b";

	/* preload /etc/resolv.conf */
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

xe_endpoint::xe_endpoint(xe_endpoint&& other){
	operator=(std::move(other));
}

xe_endpoint& xe_endpoint::operator=(xe_endpoint&& other){
	free();

	inet_ = std::move(other.inet_);
	inet6_ = std::move(other.inet6_);

	return *this;
}

const xe_slice<const in_addr>& xe_endpoint::inet() const{
	return (xe_slice<const in_addr>&)inet_;
}

const xe_slice<const in6_addr>& xe_endpoint::inet6() const{
	return (xe_slice<const in6_addr>&)inet6_;
}

void xe_endpoint::free(){
	xe_dealloc(inet_.data());
}

xe_endpoint::~xe_endpoint(){
	free();
}

void xe_resolve::poll(){
	if(handle != -1 || !active)
		return;
	loop -> release(1);
	handle = loop -> poll(pollfd, EPOLLIN, this, null, 0, 0, XURL_RESOLVER);

	xe_assert(handle >= 0);
}

void xe_resolve::sockstate(xe_ptr data, int fd, int read, int write){
	xe_resolve& resolve = *(xe_resolve*)data;
	epoll_event event;

	if(!(read || write)){
		resolve.count--;

		epoll_ctl(resolve.pollfd, EPOLL_CTL_DEL, fd, null);

		return;
	}

	event.events = EPOLLRDHUP;
	event.data.fd = fd;

	if(read)
		event.events |= EPOLLIN;
	if(write)
		event.events |= EPOLLOUT;
	if(epoll_ctl(resolve.pollfd, EPOLL_CTL_MOD, fd, &event)){
		/* force error on socket, should never happen */
		shutdown(fd, SHUT_RDWR);
	}
}

int xe_resolve::sockcreate(int fd, int type, xe_ptr data){
	xe_resolve& resolve = *(xe_resolve*)data;
	epoll_event event;

	if(resolve.count == SIZE_MAX)
		return -1;
	event.events = 0;
	event.data.u64 = fd;

	if(epoll_ctl(resolve.pollfd, EPOLL_CTL_ADD, fd, &event))
		return -1;
	resolve.count++;

	return 0;
}

static int xe_ares_error(int status){
	switch(status){
		case ARES_SUCCESS:
			return XE_OK;
		case ARES_EBADNAME:
			return XE_RESOLVER_BADNAME;
		case ARES_ENODATA:
		case ARES_ENOTFOUND:
			return XE_RESOLVER_UNKNOWN_HOST;
		case ARES_ENOMEM:
			return XE_ENOMEM;
		case ARES_ECANCELLED:
		case ARES_EDESTRUCTION:
			return XE_ECANCELED;
		case ARES_ETIMEOUT:
			return XE_RESOLVER_TIMEOUT;
		case ARES_EFORMERR:
			return XE_RESOLVER_CLIENT_FAIL;
		case ARES_ESERVFAIL:
		case ARES_EREFUSED:
		case ARES_EBADRESP:
			return XE_RESOLVER_SERVER_FAIL;
		case ARES_ECONNREFUSED:
			return XE_RESOLVER_CONNREFUSED;
		case ARES_ENOTIMP:
			return XE_ENOSYS;
		default:
			return XE_ERESOLVER;
	}
}

static bool alloc_entries(size_t inet_len, size_t inet6_len, xe_slice<in_addr>& inet, xe_slice<in6_addr>& inet6){
	ptrdiff_t inet_total, inet6_total, total;

	xe_bptr endpoints;

	if(xe_overflow_mul(inet_total, inet_len, sizeof(in_addr)) ||
		xe_overflow_mul(inet6_total, inet6_len, sizeof(in6_addr)) ||
		xe_overflow_add(total, inet_total, inet6_total))
		return false;
	endpoints = xe_alloc<byte>(total);

	if(!endpoints)
		return false;
	inet = xe_slice<in_addr>((in_addr*)endpoints, inet_len);
	inet6 = xe_slice<in6_addr>((in6_addr*)(endpoints + inet_total), inet6_len);

	return true;
}

void xe_resolve::resolved(xe_ptr data, int status, int timeouts, xe_ptr ptr){
	xe_resolve_query& query = *(xe_resolve_query*)data;
	ares_addrinfo* result = (ares_addrinfo*)ptr;

	status = xe_ares_error(status);

	if(!status){
		size_t inet_len = 0, inet6_len = 0;

		ares_addrinfo_node* node;
		xe_endpoint& list = query.endpoint;

		node = result -> nodes;

		while(node){
			if(node -> ai_family == AF_INET)
				inet_len++;
			else if(node -> ai_family == AF_INET6)
				inet6_len++;
			else
				xe_notreached();
			node = node -> ai_next;
		}

		if(alloc_entries(inet_len, inet6_len, list.inet_, list.inet6_)){
			inet_len = 0;
			inet6_len = 0;
			node = result -> nodes;

			while(node){
				if(node -> ai_family == AF_INET)
					xe_tmemcpy(&list.inet_[inet_len++], &((sockaddr_in*)node -> ai_addr) -> sin_addr);
				else if(node -> ai_family == AF_INET6)
					xe_tmemcpy(&list.inet6_[inet6_len++], &((sockaddr_in6*)node -> ai_addr) -> sin6_addr);
				else
					xe_notreached();
				node = node -> ai_next;
			}
		}

		ares_freeaddrinfo(result);
	}
#ifdef XE_DEBUG
	if(status != XE_ECANCELED){
		xe_log_verbose(query.resolve, "resolved %s in %f ms, status: %s", query.host.c_str(), (xe_time_ns() - query.start) / (float)XE_NANOS_PER_MS, xe_strerror(status));

		char ip[INET6_ADDRSTRLEN];

		for(auto& addr : query.endpoint.inet()){
			inet_ntop(AF_INET, &addr, ip, sizeof(ip));
			xe_log_debug(query.resolve, "    %s", ip);
		}

		for(auto& addr : query.endpoint.inet6()){
			inet_ntop(AF_INET6, &addr, ip, sizeof(ip));
			xe_log_debug(query.resolve, "    %s", ip);
		}
	}
#endif
	/* check for ares sync callback */
	if(!query.returned){
		query.done = true;
		query.error = status;
	}else{
		query.resolve -> ctx -> resolved(query.host, std::move(query.endpoint), status);

		xe_delete(&query);
	}
}

int xe_resolve::ip_resolve(const xe_string_view& host, xe_endpoint& endpoint){
	in_addr addr;
	in6_addr addr6;

	if(host == "localhost"){
		if(!alloc_entries(1, 1, endpoint.inet_, endpoint.inet6_))
			return XE_ENOMEM;
		xe_zero(&endpoint.inet6_[0]);

		endpoint.inet_[0].s_addr = xe_htonl(0x7f000001); /* 127.0.0.1 */
		endpoint.inet6_[0].__in6_u.__u6_addr8[15] = 1; /* ::1 */

		return 0;
	}

	if(inet_pton(AF_INET, host.c_str(), &addr) == 1){
		if(!alloc_entries(1, 0, endpoint.inet_, endpoint.inet6_))
			return XE_ENOMEM;
		endpoint.inet_[0] = addr;

		return 0;
	}

	if(inet_pton(AF_INET6, host.c_str(), &addr6) == 1){
		if(!alloc_entries(0, 1, endpoint.inet_, endpoint.inet6_))
			return XE_ENOMEM;
		xe_tmemcpy(&endpoint.inet6_[0], &addr6);

		return 0;
	}

	return XE_RESOLVER_UNKNOWN_HOST;
}

xe_resolve::xe_resolve(){
	resolver = null;
	active = 0;
	flags = 0;
	count = 0;
}

int xe_resolve::init(xurl_ctx& ctx_, xe_resolve_ctx& shared){
	ares_channel channel;
	ares_options options;
	itimerspec it;
	epoll_event event;

	int pollfd_, tfd_;
	int ret;

	ctx = &ctx_;
	loop = &ctx_.loop();
	pollfd_ = -1;
	tfd_ = -1;
	handle = -1;
	channel = null;

	xe_tmemcpy(&options, &shared.priv -> options);

	options.sock_state_cb = sockstate;
	options.sock_state_cb_data = this;

	ret = ares_init_options(&channel, &options, shared.priv -> optmask | ARES_OPT_SOCK_STATE_CB);

	if(ret != ARES_SUCCESS)
		return ret == ARES_ENOMEM ? XE_ENOMEM : XE_ERESOLVER;
	pollfd_ = epoll_create(MAX_EVENTS);

	if(pollfd_ < 0)
		goto err;
	/* timeout resolves */
	tfd_ = timerfd_create(CLOCK_MONOTONIC, 0);

	if(tfd_ < 0)
		goto err;
	it.it_value.tv_sec = (uint)TIMEOUT / XE_MILLIS_PER_SEC;
	it.it_value.tv_nsec = ((uint)TIMEOUT % XE_MILLIS_PER_SEC) * XE_NANOS_PER_MS;
	it.it_interval.tv_sec = it.it_value.tv_sec;
	it.it_interval.tv_nsec = it.it_value.tv_nsec;

	if(timerfd_settime(tfd_, 0, &it, null) < 0)
		goto err;
	event.data.u64 = tfd_;
	event.events = EPOLLIN;

	if(epoll_ctl(pollfd_, EPOLL_CTL_ADD, tfd_, &event) < 0)
		goto err;
	ares_set_socket_callback(channel, sockcreate, this);

	resolver = channel;
	pollfd = pollfd_;
	tfd = tfd_;

	return 0;
err:
	if(tfd_ != -1)
		::close(tfd_);
	if(pollfd_ != -1)
		::close(pollfd_);
	if(channel)
		ares_destroy(channel);
	return xe_errno();
}

void xe_resolve::close(){
	if(active)
		stop();
	if(resolver)
		ares_destroy((ares_channel)resolver);
	if(handle != -1){
		loop -> modify_handle(handle, null, null, tfd, pollfd);
		handle = -1;

		return;
	}

	if(tfd != -1)
		::close(tfd);
	if(pollfd != -1)
		::close(pollfd);
}

int xe_resolve::start(){
	if(active)
		return XE_EALREADY;
	if(handle != -1)
		return XE_EINVAL;
	if(loop -> remain() < 2)
		return XE_ETOOMANYHANDLES;
	xe_assert(loop -> reserve(2));

	active = true;

	return 0;
}

void xe_resolve::stop(){
	if(!active)
		return;
	active = false;

	if(handle == -1){
		loop -> release(2);

		return;
	}

	int ret;

	loop -> release(1);
	ret = loop -> poll_cancel(handle, null, null, 0, 0, XE_LOOP_HANDLE_DISCARD);

	xe_assert(ret >= 0);
}

int xe_resolve::resolve(const xe_string_view& host, xe_endpoint& endpoint){
	xe_assertm(host.c_str()[host.length()] == 0, "host is not null terminated");

	int err = ip_resolve(host, endpoint);

	if(err != XE_RESOLVER_UNKNOWN_HOST){
		xe_log_debug(this, "ipresolve %s, status: %s", host.c_str(), xe_strerror(err));

		return err;
	}

	xe_resolve_query* query = xe_new<xe_resolve_query>();

	if(!query)
		return XE_ENOMEM;
	xe_log_debug(this, "resolving %s", host.c_str());

	query -> host = host;
	query -> resolve = this;

#ifdef XE_DEBUG
	query -> start = xe_time_ns();
#endif
	ares_getaddrinfo((ares_channel)resolver, query -> host.c_str(), null, &hints, (ares_addrinfo_callback)resolved, query);

	if(query -> done){
		/* finished synchronously */
		err = query -> error;

		if(!err)
			endpoint = std::move(query -> endpoint);
		xe_dealloc(query);

		return err;
	}

	query -> returned = true;

	if(count)
		poll();
	return XE_EINPROGRESS;
}

void xe_resolve::io(xe_loop_handle& handle, int result){
	if(!handle.user_data){
		/* close called before stop request finished */
		::close(handle.u1);
		::close(handle.u2);

		return;
	}

	xe_resolve& resolve = *(xe_resolve*)handle.user_data;

	resolve.handle = -1;

	if(!resolve.active)
		return;
	xe_assert(resolve.loop -> reserve(1));

	epoll_event events[MAX_EVENTS];
	ulong buf;

	int n;
	int rfd, wfd;

	/* process events */
	n = epoll_wait(resolve.pollfd, events, MAX_EVENTS, 0);

	if(n < 0){
		xe_assert(xe_errno() == XE_EINTR);

		return;
	}

	xe_log_trace(&resolve, "processing %i events", n);

	for(int i = 0; i < n; i++){
		if(events[i].data.fd == resolve.tfd){
			rfd = ARES_SOCKET_BAD;
			wfd = ARES_SOCKET_BAD;

			xe_asserteq(read(resolve.tfd, &buf, sizeof(buf)), sizeof(buf));
		}else{
			rfd = events[i].events & (EPOLLIN | EPOLLERR) ? events[i].data.fd : ARES_SOCKET_BAD;
			wfd = events[i].events & (EPOLLOUT | EPOLLERR) ? events[i].data.fd : ARES_SOCKET_BAD;
		}

		ares_process_fd((ares_channel)resolve.resolver, rfd, wfd);
	}

	if(resolve.count)
		resolve.poll();
}

xe_cstr xe_resolve::class_name(){
	return "xe_resolve";
}