#include <unistd.h>
#include <arpa/inet.h>
#include <ares.h>
#include "xutil/overflow.h"
#include "xutil/endian.h"
#include "xutil/mem.h"
#include "xutil/log.h"
#include "xe/clock.h"
#include "xe/error.h"
#include "resolve.h"

using namespace xurl;

enum{
	TIMEOUT = 5000
};

static ares_socket_t ares_socket(int af, int type, int proto, xe_ptr data){
	return socket(af, type, proto);
}

static int ares_close(int fd, xe_ptr data){
	/*
	 * we cannot close the socket until we know
	 * the poll request will not be started anymore.
	 * the period between when a poll callback gets called and
	 * when the next poll request gets queued is when the kernel
	 * does not hold a reference to our file descriptor,
	 * and closing it during this time could poll another file
	 * or a non-existent one
	 */
	auto& handles = *(xe_rbtree<xe_resolve_node>*)data;
	auto it = handles.find(xe_resolve_node(fd));

	return it == handles.end() ? close(fd) : 0;
}

static int ares_connect(ares_socket_t fd, const sockaddr* addr, ares_socklen_t addrlen, xe_ptr data){
	return connect(fd, addr, addrlen);
}

static ares_ssize_t ares_recvfrom(ares_socket_t fd, xe_ptr buf, size_t len, int flags, sockaddr* addr, ares_socklen_t* addrlen, xe_ptr data){
	return recvfrom(fd, buf, len, flags, addr, addrlen);
}

static ares_ssize_t ares_sendv(ares_socket_t fd, const iovec* iov, int iovlen, xe_ptr data){
	return writev(fd, iov, iovlen);
}

static const ares_socket_functions socket_funcs{
	ares_socket,
	ares_close,
	ares_connect,
	ares_recvfrom,
	ares_sendv
};

struct xe_resolve_ctx::xe_resolve_ctx_data{
	ares_options options;
	int optmask;
};

struct xe_resolve_query{
	xe_string host;
	xe_endpoint endpoint;
	xe_resolve_cb callback;
	xe_ptr user;

	int error;
	bool done: 1;
	bool returned: 1;

#ifdef XE_DEBUG
	xe_resolve* resolve;
	ulong start;
#endif
};

static int xe_ares_error(int status){
	switch(status){
		case ARES_SUCCESS:
			return XE_OK;
		case ARES_EBADNAME:
			return XE_RESOLVER_BADNAME;
		case ARES_ENODATA:
		case ARES_ENOTFOUND:
			return XE_UNKNOWN_HOST;
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
			return XE_RESOLVER;
	}
}

int xe_resolve_ctx::init(){
	ares_channel channel;
	xe_resolve_ctx_data* data;

	int ret;

	data = xe_zalloc<xe_resolve_ctx_data>();

	if(!data)
		return XE_ENOMEM;
	data -> options.timeout = TIMEOUT;
	data -> options.tries = 3;
	data -> options.lookups = (char*)"b";

	/* preload /etc/resolv.conf */
	ret = ares_init_options(&channel, &data -> options, ARES_OPT_TIMEOUTMS | ARES_OPT_LOOKUPS | ARES_OPT_TRIES);

	if(ret != ARES_SUCCESS){
		xe_dealloc(data);

		return xe_ares_error(ret);
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
	ares_destroy_options(&priv -> options);
	xe_dealloc(priv);
}

xe_endpoint::xe_endpoint(xe_endpoint&& other){
	operator=(std::move(other));
}

xe_endpoint& xe_endpoint::operator=(xe_endpoint&& other){
	clear();

	inet_ = std::move(other.inet_);
	inet6_ = std::move(other.inet6_);

	return *this;
}

const xe_slice<in_addr>& xe_endpoint::inet() const{
	return inet_;
}

const xe_slice<in6_addr>& xe_endpoint::inet6() const{
	return inet6_;
}

void xe_endpoint::clear(){
	xe_dealloc(inet_.data());

	inet_.clear();
	inet6_.clear();
}

xe_endpoint::~xe_endpoint(){
	clear();
}

static void close_poll_handle(xe_poll& poll){
	poll.close();

	/* force error on later read/write */
	xe_assertz(shutdown(poll.fd(), SHUT_RDWR));
}

void xe_resolve::poll_cb(xe_poll& poll, int events){
	xe_resolve& resolve = *xe_containerof(poll, &xe_resolve_handle::poll).resolve;

	int rfd = poll.fd();
	int wfd = poll.fd();

	if(events >= 0){
		if(!(events & XE_POLL_IN))
			rfd = ARES_SOCKET_BAD;
		if(!(events & XE_POLL_OUT))
			wfd = ARES_SOCKET_BAD;
	}else{
		close_poll_handle(poll);
	}

	ares_process_fd((ares_channel)resolve.resolver, rfd, wfd);
}

void xe_resolve::close_cb(xe_poll& poll){
	xe_resolve_handle& handle = xe_containerof(poll, &xe_resolve_handle::poll);
	xe_resolve& resolve = *handle.resolve;

	resolve.handles.erase(handle.node);

	::close(poll.fd());
	xe_delete(&handle);

	if(resolve.handles.size())
		return;
	xe_assertz(resolve.loop_ -> cancel(resolve.timer));

	if(!resolve.closing)
		return;
	resolve.closing = false;

	if(resolve.close_callback) resolve.close_callback(resolve);
}

void xe_resolve::sockstate(xe_ptr data, int fd, int read, int write){
	xe_resolve& resolve = *(xe_resolve*)data;
	auto it = resolve.handles.find(fd);
	xe_resolve_handle& handle = xe_containerof(*it, &xe_resolve_handle::node);
	uint events;

	xe_assert(it != resolve.handles.end());

	if(!(read || write)){
		if(!handle.poll.close())
			close_cb(handle.poll);
		else
			handle.poll.close_callback = close_cb;
		return;
	}

	events = 0;

	if(read)
		events |= XE_POLL_IN;
	if(write)
		events |= XE_POLL_OUT;
	if(handle.poll.poll(events))
		close_poll_handle(handle.poll);
}

int xe_resolve::sockcreate(int fd, int type, xe_ptr data){
	xe_resolve& resolve = *(xe_resolve*)data;
	xe_resolve_handle* handle = xe_znew<xe_resolve_handle>();

	if(!handle)
		return -1;
	handle -> node.fd = fd;
	handle -> poll.set_loop(*resolve.loop_);
	handle -> poll.set_fd(fd);
	handle -> poll.poll_callback = poll_cb;
	handle -> resolve = &resolve;

	if(!resolve.handles.size()){
		/* timeout resolves */
		xe_assertz(resolve.loop_ -> timer_ms(resolve.timer, TIMEOUT, TIMEOUT, XE_TIMER_REPEAT));
	}

	resolve.handles.insert(handle -> node);

	return 0;
}

static bool alloc_entries(size_t inet_len, size_t inet6_len, xe_slice<in_addr>& inet, xe_slice<in6_addr>& inet6){
	ptrdiff_t inet_total, inet6_total, total;
	byte* endpoints;

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

static int make_endpoint(ares_addrinfo& result, xe_slice<in_addr>& inet, xe_slice<in6_addr>& inet6){
	size_t inet_len = 0, inet6_len = 0;
	ares_addrinfo_node* node;

	node = result.nodes;

	while(node){
		if(node -> ai_family == AF_INET)
			inet_len++;
		else if(node -> ai_family == AF_INET6)
			inet6_len++;
		else
			xe_notreached();
		node = node -> ai_next;
	}

	if(!alloc_entries(inet_len, inet6_len, inet, inet6))
		return XE_ENOMEM;
	inet_len = 0;
	inet6_len = 0;
	node = result.nodes;

	while(node){
		if(node -> ai_family == AF_INET)
			xe_tmemcpy(&inet[inet_len++], &((sockaddr_in*)node -> ai_addr) -> sin_addr);
		else if(node -> ai_family == AF_INET6)
			xe_tmemcpy(&inet6[inet6_len++], &((sockaddr_in6*)node -> ai_addr) -> sin6_addr);
		else
			xe_notreached();
		node = node -> ai_next;
	}

	return 0;
}

void xe_resolve::resolved(xe_ptr data, int status, int timeouts, xe_ptr ptr){
	xe_resolve_query& query = *(xe_resolve_query*)data;
	ares_addrinfo* result = (ares_addrinfo*)ptr;

	status = xe_ares_error(status);

	if(!status){
		status = make_endpoint(*result, query.endpoint.inet_, query.endpoint.inet6_);

		ares_freeaddrinfo(result);
	}

#ifdef XE_DEBUG
	xe_log_verbose(query.resolve,
		status ? "failed to resolve %s in %.3f ms, status: %s" : "resolved %s in %.3f ms, status: %s",
		query.host.c_str(), (xe_time_ns() - query.start) / (float)XE_NANOS_PER_MS, xe_strerror(status)
	);

	if(!status){
		char ip[INET6_ADDRSTRLEN];

		if(query.endpoint.inet().size())
			xe_log_debug(query.resolve, ">> A");
		for(auto& addr : query.endpoint.inet()){
			inet_ntop(AF_INET, &addr, ip, sizeof(ip));
			xe_log_debug(query.resolve, ">>  %s", ip);
		}

		if(query.endpoint.inet6().size())
			xe_log_debug(query.resolve, ">> AAAA");
		for(auto& addr : query.endpoint.inet6()){
			inet_ntop(AF_INET6, &addr, ip, sizeof(ip));
			xe_log_debug(query.resolve, ">>  %s", ip);
		}
	}
#endif

	/* check for ares sync callback */
	if(!query.returned){
		query.done = true;
		query.error = status;
	}else{
		query.callback(query.user, query.host, std::move(query.endpoint), status);

		xe_delete(&query);
	}
}

int xe_resolve::ip_resolve(const xe_string& host, xe_endpoint& endpoint){
	in_addr addr;
	in6_addr addr6;

	if(host == "localhost"){
		if(!alloc_entries(1, 1, endpoint.inet_, endpoint.inet6_))
			return XE_ENOMEM;
		xe_zero(&endpoint.inet6_[0]);

		endpoint.inet_[0].s_addr = xe_hton<uint>(0x7f000001); /* 127.0.0.1 */
		endpoint.inet6_[0].__in6_u.__u6_addr8[15] = 1; /* ::1 */

		return 0;
	}

	if(inet_pton(AF_INET, host.c_str(), &addr) == 1){
		/* ipv4 address */
		if(!alloc_entries(1, 0, endpoint.inet_, endpoint.inet6_))
			return XE_ENOMEM;
		endpoint.inet_[0] = addr;

		return 0;
	}

	if(inet_pton(AF_INET6, host.c_str(), &addr6) == 1){
		/* ipv6 address */
		if(!alloc_entries(0, 1, endpoint.inet_, endpoint.inet6_))
			return XE_ENOMEM;
		xe_tmemcpy(&endpoint.inet6_[0], &addr6);

		return 0;
	}

	return XE_UNKNOWN_HOST;
}

int xe_resolve::timeout(xe_loop& loop, xe_timer& timer){
	xe_resolve& resolve = xe_containerof(timer, &xe_resolve::timer);

	ares_process_fd((ares_channel)resolve.resolver, ARES_SOCKET_BAD, ARES_SOCKET_BAD);

	return 0;
}

int xe_resolve::init(xe_loop& loop, const xe_resolve_ctx& shared){
	ares_channel channel;
	ares_options options;
	int ret;

	xe_tmemcpy(&options, &shared.priv -> options);

	options.sock_state_cb = sockstate;
	options.sock_state_cb_data = this;

	ret = ares_init_options(&channel, &options, shared.priv -> optmask | ARES_OPT_SOCK_STATE_CB);

	if(ret != ARES_SUCCESS)
		return xe_ares_error(ret);
	ares_set_socket_callback(channel, sockcreate, this);
	ares_set_socket_functions(channel, &socket_funcs, &handles);

	resolver = channel;
	loop_ = &loop;

	return 0;
}

int xe_resolve::close(){
	if(closing)
		return XE_EALREADY;
	closing = true;

	ares_destroy((ares_channel)resolver);

	if(handles.size())
		return XE_EINPROGRESS;
	closing = false;

	return 0;
}

int xe_resolve::resolve(const xe_string& host, xe_endpoint& endpoint, xe_resolve_cb callback, xe_ptr user){
	ares_addrinfo_hints hints{ARES_AI_NOSORT, AF_UNSPEC, 0, 0};
	xe_resolve_query* query;
	int err;

	err = ip_resolve(host, endpoint);

	if(err != XE_UNKNOWN_HOST){
		xe_log_debug(this, "ipresolve %s, status: %s", host.c_str(), xe_strerror(err));

		return err;
	}

	query = xe_znew<xe_resolve_query>();

	if(!query)
		return XE_ENOMEM;
	if(!query -> host.copy(host))
		return XE_ENOMEM;

#ifdef XE_DEBUG
	query -> resolve = this;
	query -> start = xe_time_ns();
#endif

	xe_log_debug(this, "resolving %s", host.c_str());
	ares_getaddrinfo((ares_channel)resolver, query -> host.c_str(), null, &hints, (ares_addrinfo_callback)resolved, query);

	if(query -> done){
		/* finished synchronously */
		err = query -> error;

		if(!err)
			endpoint = std::move(query -> endpoint);
		xe_delete(query);

		return err;
	}

	query -> callback = callback;
	query -> user = user;
	query -> returned = true;

	return XE_EINPROGRESS;
}

xe_cstr xe_resolve::class_name(){
	return "xe_resolve";
}