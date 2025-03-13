#include <linux/version.h>
#include "loop.h"
#include "clock.h"
#include "xutil/mem.h"
#include "xutil/log.h"
#include "xutil/assert.h"

enum{
	ENTRY_COUNT = 256, /* default sqe and cqe count */

	/* useful in the case that not all sqes were submitted and no events return in time */
	MAX_WAIT = 4'000'000'000 /* 4 seconds */
};

void xe_promise::complete(xe_req& req, int result, uint flags){
	xe_promise& promise = (xe_promise&)req;

	promise.result_ = result;
	promise.flags_ = flags;
	promise.ready_ = true;

	if(promise.waiter) promise.waiter.resume();
}

xe_promise::xe_promise(){
	event = complete;
	waiter = null;
	result_ = 0;
	flags_ = 0;
}

static inline int xe_ring_enter(io_uring& ring, uint submit, uint wait, uint flags, ulong timeout){
	io_uring_getevents_arg args;
	__kernel_timespec ts;

	/*
	 * the kernel doesn't read the timespec until it's actually time to wait for cqes
	 * avoid loss due to branching here and set EXT_ARG on every enter
	 * even if we're not waiting for events
	 */
	flags |= IORING_ENTER_EXT_ARG;

	args.sigmask = 0;
	args.sigmask_sz = _NSIG / 8;
	args.ts = (ulong)&ts;
	args.pad = 0;

	ts.tv_nsec = timeout;
	ts.tv_sec = 0;

	return io_uring_enter2(ring.ring_fd, submit, wait, flags, (sigset_t*)&args, sizeof(args));
}

static inline bool xe_cqe_test_flags(io_uring& ring, uint flags){
	return IO_URING_READ_ONCE(*ring.sq.kflags) & flags ? true : false;
}

static inline bool xe_cqe_needs_enter(io_uring& ring){
	return xe_cqe_test_flags(ring, IORING_SQ_CQ_OVERFLOW | IORING_SQ_TASKRUN);
}

static inline bool xe_cqe_needs_flush(io_uring& ring){
	return xe_cqe_test_flags(ring, IORING_SQ_CQ_OVERFLOW);
}

static inline uint xe_cqe_available(io_uring& ring){
	uint cqe_head;
	uint cqe_tail;

	cqe_head = *ring.cq.khead;
	cqe_tail = io_uring_smp_load_acquire(ring.cq.ktail);

	return cqe_tail - cqe_head;
}

inline int xe_loop::submit(bool want_events){
	uint submit, wait;
	uint flags;
	int res;

	ulong timeout;

	submit = queued_;
	flags = 0;
	wait = 0;

	if(!want_events){
		/* we want to flush cqring if possible, but not run any task work */
		if(xe_cqe_needs_flush(ring)) [[unlikely]]
			flags |= IORING_ENTER_GETEVENTS;
	}else do{
		xe_rbtree<xe_timer>::iterator it;
		ulong now;

		if(!(submit | handles_ | active_timers)) [[unlikely]] {
			/* done */
			return XE_ENOENT;
		}

		if(sq_ring_full) [[unlikely]] {
			/* not enough memory to submit more, just wait for returns */
			submit = 0;
		}

		if(!submit && xe_cqe_available(ring)){
			/* events available */
			return 0;
		}

		flags |= IORING_ENTER_GETEVENTS;

		if(ring.flags & IORING_SETUP_IOPOLL) [[unlikely]] {
			/* iopoll does not block, no need to calculate timeout */
			break;
		}

		now = xe_time_ns();
		it = timers.begin();
		timeout = MAX_WAIT;
		wait = 1;

		if(it != timers.end()){
			/* we may have to exit early to run the timer */
			if(now < it -> expire)
				timeout = xe_min<ulong>(it -> expire - now, MAX_WAIT);
			else{
				/* if timer already expired, just submit and/or flush cqe, don't wait */
				wait = 0;
			}
		}

		if(submit || wait) [[likely]]
			break;
		if(xe_cqe_needs_enter(ring)) [[likely]] {
			/* events need to be flushed or task worked */
			break;
		}

		/* nothing to do */
		return 0;
	}while(false);

	do{
		if(want_events && !submit) [[unlikely]] {
			/* only getevents */
			break;
		}

		/* submit sqes */
		if(!(ring.flags & IORING_SETUP_SQPOLL)) [[likely]] {
			IO_URING_WRITE_ONCE(*ring.sq.ktail, ring.sq.sqe_tail);

			break;
		}

		io_uring_smp_store_release(ring.sq.ktail, ring.sq.sqe_tail);
		io_uring_smp_mb();

		if(IO_URING_READ_ONCE(*ring.sq.kflags) & IORING_SQ_NEED_WAKEUP) [[unlikely]]
			flags |= IORING_ENTER_SQ_WAKEUP;
		if(flags)
			break;
		res = submit;

		goto sqpoll_done;
	}while(false);

	res = xe_ring_enter(ring, submit, wait, flags, timeout);

	if(res >= 0) [[likely]] {
	sqpoll_done:
		sq_ring_full = false;

		if(!want_events || submit) [[likely]] {
			xe_log_trace(this, "<< ring %u", res);

			if(!res) [[unlikely]] {
				/* didn't submit anything but kernel has memory for the next submit */
				return want_events ? 0 : XE_EAGAIN;
			}

			queued_ -= res;
			handles_ += res;
		}

		return 0;
	}

	if(res == XE_ETIME || res == XE_EBUSY || res == XE_EINTR) [[likely]]
		return 0;
	if(res == XE_EAGAIN){
		xe_log_debug(this, ">> ring queue full");

		sq_ring_full = true;

		return want_events ? 0 : XE_EAGAIN;
	}

	xe_log_error(this, ">> ring fatal error: %s", xe_strerror(res));

	return res == XE_EBADR ? XE_ENOMEM : XE_FATAL;
}

void xe_loop::run_timer(xe_timer& timer, ulong now){
	ulong align, delay;

	erase_timer(timer);

	timer.in_callback = true;

	if(timer.callback(*this, timer)){
		/* timer no longer accessible */
		return;
	}

	timer.in_callback = false;

	if(timer.active_ || !timer.repeat_){
		/*
		 * timer active: enqueued in callback
		 * not repeating: nothing left to do
		 */
		return;
	}

	delay = timer.delay;

	if(!delay)
		now++;
	else if(!timer.align_)
		now += delay;
	else{
		/* find how much we overshot */
		if(now <= timer.expire + delay)
			now = timer.expire + delay;
		else{
			align = (now - timer.expire) % delay;
			/* subtract the overshot */
			now += delay - align;
		}
	}

	timer.expire = now;

	queue_timer(timer);
}

void xe_loop::queue_timer(xe_timer& timer){
	timer.active_ = true;

	if(!timer.passive_)
		active_timers++;
	timers.insert(timer);
}

void xe_loop::erase_timer(xe_timer& timer){
	timer.active_= false;

	if(!timer.passive_)
		active_timers--;
	timers.erase(timer);
}

inline int xe_loop::queue_io(xe_req_info& info){
	io_uring_sqe* sqe;
	int res;

	xe_assert(queued_ <= capacity());

	if(queued_ < capacity()) [[likely]]
		goto enqueue;
	res = submit(false);

	if(!res)
		goto enqueue;
	return res;
enqueue:
	queued_++;
	sqe = &ring.sq.sqes[ring.sq.sqe_tail++ & ring.sq.ring_mask];

	/* copy io parameters */
	*sqe = info.op.sqe;

	return 0;
}

int xe_loop::queue_pending(){
	/* submit deferred requests */
	int err;

	if(!reqs) [[likely]]
		return 0;
	if(sq_ring_full) [[unlikely]] {
		/* can't queue any more right now */
		return 0;
	}

	while(reqs){
		err = queue_io(reqs.front());

		if(!err)
			reqs.erase(reqs.front());
		else if(err != XE_EAGAIN)
			return err;
		else
			break;
	}

	return 0;
}

int xe_loop::get_sqe(xe_req& req, io_uring_sqe*& sqe, xe_req_info* info){
	int res;

	xe_assert(queued_ <= capacity());
	xe_return_error(error);

	do{
		if(queued_ < capacity()) [[likely]]
			break;
		/*
		 * sq ring is filled,
		 * try freeing up a spot to submit I/O.
		 * upon failure, put the request in a queue
		 * and try again when there are free sqes
		 */
		if(sq_ring_full) [[unlikely]]
			res = XE_EAGAIN;
		else
			res = submit(false);
		if(!res) [[likely]] {
			/* submit success, go queue */
			break;
		}

		if(res != XE_EAGAIN) [[unlikely]] {
			error = res;

			return res;
		}

		if(!info)
			return XE_ENOMEM;
		reqs.append(*info);
		sqe = &info -> op.sqe;

		return 0;
	}while(false);

	/* get the next sqe */
	queued_++;
	sqe = &ring.sq.sqes[ring.sq.sqe_tail++ & ring.sq.ring_mask];

	return 0;
}

int xe_loop::init(uint entries){
	xe_loop_options options;

	options.entries = entries;

	return init_options(options);
}

int xe_loop::init_options(xe_loop_options& options){
	io_uring_params params;
	byte* io_buf_;
	int err;

	if(!options.entries)
		options.entries = ENTRY_COUNT;
	xe_zero(&params);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0)
	#error "Kernel version too low"
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	params.flags |= IORING_SETUP_SUBMIT_ALL;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
	params.flags |= IORING_SETUP_COOP_TASKRUN | IORING_SETUP_TASKRUN_FLAG;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
	params.flags |= IORING_SETUP_SINGLE_ISSUER;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	params.flags |= IORING_SETUP_DEFER_TASKRUN;
#endif

	if(options.flag_iopoll)
		params.flags |= IORING_SETUP_IOPOLL;
	if(options.flag_sqpoll)
		params.flags |= IORING_SETUP_SQPOLL;
	if(options.flag_sqaff){
		params.flags |= IORING_SETUP_SQ_AFF;
		params.sq_thread_cpu = options.sq_thread_cpu;
	}

	if(options.flag_cqsize){
		params.flags |= IORING_SETUP_CQSIZE;
		params.cq_entries = options.cq_entries;

		if(!params.cq_entries) params.cq_entries = options.entries << 1;
	}

	if(options.flag_attach_wq){
		params.flags |= IORING_SETUP_ATTACH_WQ;
		params.wq_fd = options.wq_fd;
	}

	io_buf_ = null;

	if(options.flag_iobuf){
		io_buf_ = xe_alloc_aligned<byte>(0, XE_LOOP_IOBUF_SIZE_LARGE);

		if(!io_buf_) return XE_ENOMEM;
		/* force memory allocation */
		xe_zero(io_buf_, XE_LOOP_IOBUF_SIZE_LARGE);
	}

	err = io_uring_queue_init_params(options.entries, &ring, &params);

	if(err)
		xe_dealloc(io_buf_);
	else
		io_buf = io_buf_;
	return err;
}

void xe_loop::close(){
	io_uring_queue_exit(&ring);
	xe_dealloc(io_buf);
}

int xe_loop::run(){
	ulong now;
	int res;

	uint cqe_head;
	uint cqe_tail;
	uint cqe_mask;

	cqe_head = *ring.cq.khead;
	cqe_mask = ring.cq.ring_mask;

	while(true){
		xe_return_error(queue_pending());

		res = submit(true);

		if(res) [[unlikely]]
			goto exit;
		now = xe_time_ns();

		/* process outstanding timers */
		for(auto it = timers.begin(); it != timers.end(); it = timers.begin()){
			xe_timer& timer = *it;

			if(now < timer.expire)
				break;
			run_timer(timer, now);

			if(error) [[unlikely]]
				goto exit_error;
		}

		cqe_tail = *ring.cq.ktail;

		if(cqe_tail == cqe_head)
			continue;
		/* pending reqs take priority */
		xe_return_error(queue_pending());
		xe_log_trace(this, ">> ring %u", cqe_tail - cqe_head);

		uint* khead = ring.cq.khead;
		io_uring_cqe* cqring = ring.cq.cqes;

		/* process events */
		do{
			io_uring_cqe* cqe;
			xe_req* req;
			uint res, flags;

			cqe = &cqring[cqe_head++ & cqe_mask];
			req = (xe_req*)cqe -> user_data;
			res = cqe -> res;
			flags = cqe -> flags;

			/*
			 * more requests may be queued in callback, so
			 * update the cqe head here so that we have one more cqe
			 * available for completions before overflow occurs
			 */
			io_uring_smp_store_release(khead, cqe_head);

			if(!(flags & IORING_CQE_F_MORE)) [[likely]]
				handles_--;
			if(req -> event) [[likely]]
				req -> event(*req, res, flags);
			if(error) [[unlikely]]
				goto exit_error;
		}while(cqe_tail != cqe_head);
	}
exit:
	return res == XE_ENOENT ? 0 : res;
exit_error:
	return error;
}

uint xe_loop::sqe_count() const{
	return ring.sq.ring_entries;
}

uint xe_loop::cqe_count() const{
	return ring.cq.ring_entries;
}

uint xe_loop::remain() const{
	return capacity() - queued_;
}

uint xe_loop::capacity() const{
	return sqe_count();
}

uint xe_loop::queued() const{
	return queued_;
}

ulong xe_loop::handles() const{
	return handles_;
}

int xe_loop::flush(){
	if(!queued_) [[unlikely]]
		return 0;
	xe_return_error(error);

	if(sq_ring_full) [[unlikely]]
		return XE_EAGAIN;
	error = submit(false);

	return error;
}

int xe_loop::timer_ms(xe_timer& timer, ulong millis, ulong repeat, uint flags){
	return timer_ns(timer, millis * XE_NANOS_PER_MS, repeat * XE_NANOS_PER_MS, flags);
}

int xe_loop::timer_ns(xe_timer& timer, ulong nanos, ulong repeat, uint flags){
	if(timer.active_)
		return XE_EALREADY;
	if(!(flags & XE_TIMER_ABS))
		nanos += xe_time_ns();
	timer.delay = repeat;
	timer.repeat_ = flags & XE_TIMER_REPEAT ? true : false;
	timer.align_ = flags & XE_TIMER_ALIGN ? true : false;
	timer.passive_ = flags & XE_TIMER_PASSIVE ? true : false;
	timer.expire = nanos;

	queue_timer(timer);

	return 0;
}

int xe_loop::cancel(xe_timer& timer){
	/* a timer in callback is not active */
	if(timer.in_callback){
		timer.repeat_ = false; /* won't be started again */

		return 0;
	}

	if(!timer.active_)
		return XE_ENOENT;
	timer.repeat_ = false;

	erase_timer(timer);

	return 0;
}

xe_ptr xe_loop::iobuf() const{
	return io_buf;
}

xe_ptr xe_loop::iobuf_large() const{
	return io_buf;
}

int xe_loop::register_buffers(const iovec* iov, uint vlen){
	return io_uring_register_buffers(&ring, iov, vlen);
}

int xe_loop::register_buffers_sparse(uint len){
	return io_uring_register_buffers_sparse(&ring, len);
}

int xe_loop::register_buffers_update_tag(uint off, const iovec* iov, const ulong* tags, uint len){
	return io_uring_register_buffers_update_tag(&ring, off, iov, (__u64*)tags, len);
}

int xe_loop::unregister_buffers(){
	return io_uring_unregister_buffers(&ring);
}

int xe_loop::register_files(const int* fds, uint len){
	return io_uring_register_files(&ring, fds, len);
}

int xe_loop::register_files_tags(const int* fds, const ulong* tags, uint len){
	return io_uring_register_files_tags(&ring, fds, (__u64*)tags, len);
}

int xe_loop::register_files_sparse(uint len){
	return io_uring_register_files_sparse(&ring, len);
}

int xe_loop::register_files_update(uint off, const int* fds, uint len){
	return io_uring_register_files_update(&ring, off, fds, len);
}

int xe_loop::register_files_update_tag(uint off, const int* fds, const ulong* tags, uint len){
	return io_uring_register_files_update_tag(&ring, off, fds, (__u64*)tags, len);
}

int xe_loop::register_file_alloc_range(uint off, uint len){
	return io_uring_register_file_alloc_range(&ring, off, len);
}

int xe_loop::unregister_files(){
	return io_uring_unregister_files(&ring);
}

xe_cstr xe_loop::class_name(){
	return "xe_loop";
}