#pragma once
#include <sys/poll.h>
#include <sys/epoll.h>
#include "xstd/types.h"
#include "xutil/util.h"
#include "../loop.h"

enum{
	XE_POLL_NONE = 0x0,
	XE_POLL_IN = POLLIN,
	XE_POLL_OUT = POLLOUT,
	XE_POLL_ERR = POLLERR,
	XE_POLL_HUP = POLLHUP,
	XE_POLL_NVAL = POLLNVAL,
	XE_POLL_RDNORM = POLLRDNORM,
	XE_POLL_RDBAND = POLLRDBAND,
	XE_POLL_WRNORM = POLLWRNORM,
	XE_POLL_MSG = POLLMSG,
	XE_POLL_RDHUP = POLLRDHUP,

	XE_POLL_EXCLUSIVE = EPOLLEXCLUSIVE,
	XE_POLL_ONESHOT = EPOLLONESHOT,
	XE_POLL_EDGE_TRIGGERED = EPOLLET
};

class xe_poll{
private:
	static void poll_cb(xe_req&, int);
	static void cancel_cb(xe_req&, int);

	int update_poll();
	void check_close();

	xe_loop* loop_;

	xe_req poll_req;
	xe_req cancel_req;
	xe_req_info cancel_info;

	int fd_;
	uint events_;

	bool active: 1;
	bool polling: 1;
	bool modifying: 1;
	bool updated: 1;
	bool restart: 1;
	bool closing: 1;
public:
	void (*poll_callback)(xe_poll& poll, int result);
	void (*close_callback)(xe_poll& poll);

	xe_poll(){
		poll_req.callback = poll_cb;
		cancel_req.callback = cancel_cb;

		active = false;
		polling = false;
		modifying = false;
		updated = false;
		restart = false;
		closing = false;

		poll_callback = null;
		close_callback = null;
	}

	xe_poll(xe_loop& loop): xe_poll(){
		loop_ = &loop;
	}

	xe_disable_copy_move(xe_poll)

	void set_loop(xe_loop& loop){
		loop_ = &loop;
	}

	xe_loop& loop() const{
		return *loop_;
	}

	int fd() const{
		return fd_;
	}

	void set_fd(int fd){
		fd_ = fd;
	}

	int poll(uint events);
	int close();

	~xe_poll() = default;
};