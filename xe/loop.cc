#include <linux/version.h>
#include "loop.h"
#include "clock.h"
#include "error.h"
#include "xutil/mem.h"
#include "xutil/endian.h"
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
	return IO_URING_READ_ONCE(*ring.sq.kflags) & (flags) ? true : false;
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
		if(xe_cqe_needs_flush(ring)) [[unlikely]]
			flags |= IORING_ENTER_GETEVENTS;
	}else do{
		xe_rbtree<ulong>::iterator it;
		ulong now;

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

		if(it != timers.end()){
			/* we may have to exit early to run the timer */
			timeout = now < it -> key ? it -> key - now : 0;
			timeout = xe_min<ulong>(timeout, MAX_WAIT); // todo test branching here
		}

		/* if timer already expired, just submit and/or flush cqe, don't wait */
		wait = timeout ? 1 : 0;

		if(submit || wait) [[likely]]
			break;
		if(xe_cqe_needs_enter(ring)) [[likely]] {
			/* events need to be flushed or task worked */
			break;
		}

		if(!handles_){
			/* only running timers, no outstanding i/o */
			return active_timers ? 0 : XE_ENOENT;
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
			io_uring_smp_store_release(ring.sq.ktail, ring.sq.sqe_tail);

			break;
		}

		IO_URING_WRITE_ONCE(*ring.sq.ktail, ring.sq.sqe_tail);
		io_uring_smp_mb();

		if(IO_URING_READ_ONCE(*ring.sq.kflags) & IORING_SQ_NEED_WAKEUP) [[unlikely]]
			flags |= IORING_ENTER_SQ_WAKEUP;
		if(flags)
			break;
		res = submit;

		goto sqpoll_done;
	}while(false);

	res = xe_ring_enter(ring, submit, wait, flags, timeout);

	if(res < 0) [[unlikely]]
		goto err;
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
err:
	if(res == XE_ETIME || res == XE_EBUSY || res == XE_EINTR) [[likely]]
		return 0;
	if(res == XE_EAGAIN){
		xe_log_debug(this, "<< ring queue full");

		sq_ring_full = true;

		return want_events ? 0 : XE_EAGAIN;
	}

	xe_log_error(this, "<< ring fatal error: %s", xe_strerror(res));

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
		if(now <= timer.expire.key + delay)
			now = timer.expire.key + delay;
		else{
			align = (now - timer.expire.key) % delay;
			/* subtract the overshot */
			now += delay - align;
		}
	}

	timer.expire.key = now;

	queue_timer(timer);
}

void xe_loop::queue_timer(xe_timer& timer){
	timer.active_ = true;

	if(!timer.passive_)
		active_timers++;
	timers.insert(timer.expire);
}

void xe_loop::erase_timer(xe_timer& timer){
	timer.active_= false;

	if(!timer.passive_)
		active_timers--;
	timers.erase(timer.expire);
}

template<class F>
inline int xe_loop::queue_io(int op, xe_req& req, F init_sqe){
	io_uring_sqe* sqe;
	xe_req_info* info;
	int res;

	xe_assert(queued_ <= capacity());
	xe_return_error(error);

	if(queued_ >= capacity()) [[unlikely]]
		goto submit;
enqueue:
	queued_++;
	sqe = &ring.sq.sqes[ring.sq.sqe_tail++ & ring.sq.ring_mask];
store:
	/* copy io parameters */
	sqe -> opcode = op;
	sqe -> user_data = (ulong)&req;

	init_sqe(*sqe);

	return 0;
submit:
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
	if(!res) [[likely]]
		goto enqueue;
	if(res != XE_EAGAIN) [[unlikely]] {
		error = res;

		return res;
	}

	info = req.info;

	if(!info)
		return XE_ENOMEM;
	reqs.append(info -> node);
	sqe = &info -> sqe;

	goto store;
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
	xe_tmemcpy(sqe, &info.sqe);

	return 0;
}

template<class F>
int xe_loop::queue_cancel(int op, xe_req& req, xe_req& cancel, F init_sqe){
	xe_req_info* info;
	int res;

	info = cancel.info;

	if(info && info -> node.in_list()){
		/* the request was put in our deferred queue */
		info -> node.erase();

		return 0;
	}

	res = queue_io(op, req, [&](io_uring_sqe& sqe){
		sqe.addr = (ulong)&cancel;
		init_sqe(sqe);
	});

	return res ?: XE_EINPROGRESS;
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
		err = queue_io(xe_containerof(reqs.head(), &xe_req_info::node));

		if(!err)
			reqs.erase(reqs.head());
		else if(err != XE_EAGAIN)
			return err;
		else
			break;
	}

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
	io_uring_cqe* cqring = ring.cq.cqes;

	cqe_head = *ring.cq.khead;
	cqe_tail = cqe_head;
	cqe_mask = ring.cq.ring_mask;

	while(true){
		xe_return_error(queue_pending());

		res = submit(true);

		if(res) [[unlikely]]
			goto exit;
		now = xe_time_ns();

		/* process outstanding timers */
		for(auto it = timers.begin(); it != timers.end(); it = timers.begin()){
			xe_timer& timer = xe_containerof(*it, &xe_timer::expire);

			if(now < timer.expire.key)
				break;
			run_timer(timer, now);

			if(error) [[unlikely]]
				goto exit_error;
		}

		cqe_tail = io_uring_smp_load_acquire(ring.cq.ktail);

		if(cqe_tail == cqe_head)
			continue;
		xe_log_trace(this, ">> ring %u", cqe_tail - cqe_head);

		/* process events */
		do{
			io_uring_cqe* cqe;
			xe_req* req;
			int res, flags;

			cqe = &cqring[cqe_head++ & cqe_mask];
			req = (xe_req*)cqe -> user_data;
			flags = cqe -> flags;
			res = cqe -> res;

			if(!(flags & IORING_CQE_F_MORE)) [[likely]]
				handles_--;
			if(req -> event) [[likely]]
				req -> event(*req, res, flags);
			if(error) [[unlikely]]
				goto exit_error;
		}while(cqe_tail != cqe_head);

		io_uring_smp_store_release(ring.cq.khead, cqe_tail);
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
	timer.expire.key = nanos;

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

static inline void xe_sqe_init(io_uring_sqe& sqe, byte sq_flags, ushort ioprio){
	sqe.flags = sq_flags;
	sqe.ioprio = ioprio;
}

static inline void xe_sqe_rw(io_uring_sqe& sqe, byte sq_flags, ushort ioprio, int fd, xe_cptr addr, uint len, ulong offset, uint rw_flags){
	xe_sqe_init(sqe, sq_flags, ioprio);

	sqe.fd = fd;
	sqe.addr = (ulong)addr;
	sqe.len = len;
	sqe.off = offset;
	sqe.rw_flags = rw_flags;
}

static inline void xe_sqe_rw_fixed(io_uring_sqe& sqe, byte sq_flags, ushort ioprio, int fd, xe_cptr addr, uint len, ulong offset, uint rw_flags, uint buf_index){
	xe_sqe_rw(sqe, sq_flags, ioprio, fd, addr, len, offset, rw_flags);

	sqe.buf_index = buf_index;
}

static inline void xe_sqe_close(io_uring_sqe& sqe, byte sq_flags, int fd, uint file_index){
	xe_sqe_rw_fixed(sqe, sq_flags, 0, fd, null, 0, 0, 0, 0);

	sqe.file_index = file_index;
}

static inline void xe_sqe_sync(io_uring_sqe& sqe, byte sq_flags, int fd, uint len, ulong offset, uint sync_flags){
	xe_sqe_rw_fixed(sqe, sq_flags, 0, fd, null, len, offset, sync_flags, 0);

	sqe.splice_fd_in = 0;
}

static inline void xe_sqe_advise(io_uring_sqe& sqe, byte sq_flags, xe_cptr addr, uint len, ulong offset, uint advice){
	xe_sqe_init(sqe, sq_flags, 0);

	sqe.addr = (ulong)addr;
	sqe.len = len;
	sqe.off = offset;
	sqe.fadvise_advice = advice;
	sqe.buf_index = 0;
	sqe.splice_fd_in = 0;
}

static inline void xe_sqe_fs(io_uring_sqe& sqe, byte sq_flags, int fd0, xe_cptr ptr0, int fd1, xe_cptr ptr1, uint fs_flags){
	xe_sqe_init(sqe, sq_flags, 0);

	sqe.fd = fd0;
	sqe.addr = (ulong)ptr0;
	sqe.len = fd1;
	sqe.off = (ulong)ptr1;
	sqe.rw_flags = fs_flags;
	sqe.buf_index = 0;
	sqe.splice_fd_in = 0;
}

static inline void xe_sqe_fxattr(io_uring_sqe& sqe, byte sq_flags, int fd, xe_cptr name, xe_cptr value, uint len, uint xattr_flags){
	xe_sqe_init(sqe, sq_flags, 0);

	sqe.fd = fd;
	sqe.addr = (ulong)name;
	sqe.len = len;
	sqe.addr2 = (ulong)value;
	sqe.xattr_flags = xattr_flags;
}

static inline void xe_sqe_xattr(io_uring_sqe& sqe, byte sq_flags, xe_cstr path, xe_cptr name, xe_cptr value, uint len, uint xattr_flags){
	xe_sqe_init(sqe, sq_flags, 0);

	sqe.addr3 = (ulong)path;
	sqe.addr = (ulong)name;
	sqe.len = len;
	sqe.addr2 = (ulong)value;
	sqe.xattr_flags = xattr_flags;
}

static inline void xe_sqe_splice(io_uring_sqe& sqe, byte sq_flags, int fd_in, ulong off_in, int fd_out, ulong off_out, uint len, uint splice_flags){
	xe_sqe_init(sqe, sq_flags, 0);

	sqe.splice_fd_in = fd_in;
	sqe.splice_off_in = off_in;
	sqe.fd = fd_out;
	sqe.off = off_out;
	sqe.len = len;
	sqe.splice_flags = splice_flags;
}

static inline void xe_sqe_socket(io_uring_sqe& sqe, byte sq_flags, int fd, xe_cptr addr, uint len, ulong offset, uint socket_flags, uint file_index){
	xe_sqe_rw_fixed(sqe, sq_flags, 0, fd, addr, len, offset, socket_flags, 0);

	sqe.file_index = file_index;
}

static inline void xe_sqe_socket_rw(io_uring_sqe& sqe, byte sq_flags, ushort ioprio, int fd, xe_cptr addr, uint len, uint msg_flags){
	xe_sqe_rw(sqe, sq_flags, ioprio, fd, addr, len, 0, msg_flags);

	sqe.file_index = 0;
}

static inline void xe_sqe_buffer(io_uring_sqe& sqe, byte sq_flags, xe_ptr addr, uint len, ushort nr, ushort bgid, ushort bid){
	xe_sqe_init(sqe, sq_flags, 0);

	sqe.fd = nr;
	sqe.addr = (ulong)addr;
	sqe.len = len;
	sqe.off = bid;
	sqe.buf_group = bgid;
	sqe.rw_flags = 0;
}

int xe_loop::nop_ex(xe_req& req, byte sq_flags){
	return queue_io(IORING_OP_NOP, req, [&](io_uring_sqe& sqe){
		xe_sqe_init(sqe, sq_flags, 0);
	});
}

int xe_loop::openat_ex(xe_req& req, byte sq_flags, int dfd, xe_cstr path, uint flags, mode_t mode, uint file_index){
	return queue_io(IORING_OP_OPENAT, req, [&](io_uring_sqe& sqe){
		xe_sqe_init(sqe, sq_flags, 0);

		sqe.fd = dfd;
		sqe.addr = (ulong)path;
		sqe.len = mode;
		sqe.open_flags = flags;
		sqe.buf_index = 0;
		sqe.file_index = file_index;
	});
}

int xe_loop::openat2_ex(xe_req& req, byte sq_flags, int dfd, xe_cstr path, open_how* how, uint file_index){
	return queue_io(IORING_OP_OPENAT2, req, [&](io_uring_sqe& sqe){
		xe_sqe_init(sqe, sq_flags, 0);

		sqe.fd = dfd;
		sqe.addr = (ulong)path;
		sqe.len = sizeof(*how);
		sqe.off = (ulong)how;
		sqe.buf_index = 0;
		sqe.file_index = file_index;
	});
}

int xe_loop::close_ex(xe_req& req, byte sq_flags, int fd){
	return queue_io(IORING_OP_CLOSE, req, [&](io_uring_sqe& sqe){
		xe_sqe_close(sqe, sq_flags, fd, 0);
	});
}

int xe_loop::close_direct_ex(xe_req& req, byte sq_flags, uint file_index){
	return queue_io(IORING_OP_CLOSE, req, [&](io_uring_sqe& sqe){
		xe_sqe_close(sqe, sq_flags, 0, file_index);
	});
}

int xe_loop::read_ex(xe_req& req, byte sq_flags, ushort ioprio, int fd, xe_ptr buf, uint len, long offset, uint flags){
	return queue_io(IORING_OP_READ, req, [&](io_uring_sqe& sqe){
		xe_sqe_rw(sqe, sq_flags, ioprio, fd, buf, len, offset, flags);
	});
}

int xe_loop::write_ex(xe_req& req, byte sq_flags, ushort ioprio, int fd, xe_cptr buf, uint len, long offset, uint flags){
	return queue_io(IORING_OP_WRITE, req, [&](io_uring_sqe& sqe){
		xe_sqe_rw(sqe, sq_flags, ioprio, fd, buf, len, offset, flags);
	});
}

int xe_loop::readv_ex(xe_req& req, byte sq_flags, ushort ioprio, int fd, const iovec* vecs, uint vlen, long offset, uint flags){
	return queue_io(IORING_OP_READV, req, [&](io_uring_sqe& sqe){
		xe_sqe_rw(sqe, sq_flags, ioprio, fd, vecs, vlen, offset, flags);
	});
}

int xe_loop::writev_ex(xe_req& req, byte sq_flags, ushort ioprio, int fd, const iovec* vecs, uint vlen, long offset, uint flags){
	return queue_io(IORING_OP_WRITEV, req, [&](io_uring_sqe& sqe){
		xe_sqe_rw(sqe, sq_flags, ioprio, fd, vecs, vlen, offset, flags);
	});
}

int xe_loop::read_fixed_ex(xe_req& req, byte sq_flags, ushort ioprio, int fd, xe_ptr buf, uint len, long offset, uint buf_index, uint flags){
	return queue_io(IORING_OP_READ_FIXED, req, [&](io_uring_sqe& sqe){
		xe_sqe_rw_fixed(sqe, sq_flags, ioprio, fd, buf, len, offset, flags, buf_index);
	});
}

int xe_loop::write_fixed_ex(xe_req& req, byte sq_flags, ushort ioprio, int fd, xe_cptr buf, uint len, long offset, uint buf_index, uint flags){
	return queue_io(IORING_OP_WRITE_FIXED, req, [&](io_uring_sqe& sqe){
		xe_sqe_rw_fixed(sqe, sq_flags, ioprio, fd, buf, len, offset, flags, buf_index);
	});
}

int xe_loop::fsync_ex(xe_req& req, byte sq_flags, int fd, uint flags){
	return queue_io(IORING_OP_FSYNC, req, [&](io_uring_sqe& sqe){
		xe_sqe_sync(sqe, sq_flags, fd, 0, 0, flags);
	});
}

int xe_loop::sync_file_range_ex(xe_req& req, byte sq_flags, int fd, uint len, long offset, uint flags){
	return queue_io(IORING_OP_SYNC_FILE_RANGE, req, [&](io_uring_sqe& sqe){
		xe_sqe_sync(sqe, sq_flags, fd, len, offset, flags);
	});
}

int xe_loop::fallocate_ex(xe_req& req, byte sq_flags, int fd, int mode, long offset, long len){
	return queue_io(IORING_OP_FALLOCATE, req, [&](io_uring_sqe& sqe){
		xe_sqe_rw_fixed(sqe, sq_flags, 0, fd, (xe_ptr)len, mode, offset, 0, 0);

		sqe.splice_fd_in = 0;
	});
}

int xe_loop::fadvise_ex(xe_req& req, byte sq_flags, int fd, ulong offset, uint len, uint advice){
	return queue_io(IORING_OP_FADVISE, req, [&](io_uring_sqe& sqe){
		xe_sqe_advise(sqe, sq_flags, null, len, offset, advice);

		sqe.fd = fd;
	});
}

int xe_loop::madvise_ex(xe_req& req, byte sq_flags, xe_ptr addr, uint len, uint advice){
	return queue_io(IORING_OP_MADVISE, req, [&](io_uring_sqe& sqe){
		xe_sqe_advise(sqe, sq_flags, addr, len, 0, advice);
	});
}

int xe_loop::renameat_ex(xe_req& req, byte sq_flags, int old_dfd, xe_cstr old_path, int new_dfd, xe_cstr new_path, uint flags){
	return queue_io(IORING_OP_RENAMEAT, req, [&](io_uring_sqe& sqe){
		xe_sqe_fs(sqe, sq_flags, old_dfd, old_path, new_dfd, new_path, flags);
	});
}

int xe_loop::unlinkat_ex(xe_req& req, byte sq_flags, int dfd, xe_cstr path, uint flags){
	return queue_io(IORING_OP_UNLINKAT, req, [&](io_uring_sqe& sqe){
		xe_sqe_fs(sqe, sq_flags, dfd, path, 0, null, flags);
	});
}

int xe_loop::mkdirat_ex(xe_req& req, byte sq_flags, int dfd, xe_cstr path, mode_t mode){
	return queue_io(IORING_OP_MKDIRAT, req, [&](io_uring_sqe& sqe){
		xe_sqe_fs(sqe, sq_flags, dfd, path, mode, null, 0);
	});
}

int xe_loop::symlinkat_ex(xe_req& req, byte sq_flags, xe_cstr target, int newdirfd, xe_cstr linkpath){
	return queue_io(IORING_OP_SYMLINKAT, req, [&](io_uring_sqe& sqe){
		xe_sqe_fs(sqe, sq_flags, newdirfd, target, 0, linkpath, 0);
	});
}

int xe_loop::linkat_ex(xe_req& req, byte sq_flags, int old_dfd, xe_cstr old_path, int new_dfd, xe_cstr new_path, uint flags){
	return queue_io(IORING_OP_LINKAT, req, [&](io_uring_sqe& sqe){
		xe_sqe_fs(sqe, sq_flags, old_dfd, old_path, new_dfd, new_path, flags);
	});
}

int xe_loop::fgetxattr_ex(xe_req& req, byte sq_flags, int fd, xe_cstr name, char* value, uint len){
	return queue_io(IORING_OP_FGETXATTR, req, [&](io_uring_sqe& sqe){
		xe_sqe_fxattr(sqe, sq_flags, fd, name, value, len, 0);
	});
}

int xe_loop::fsetxattr_ex(xe_req& req, byte sq_flags, int fd, xe_cstr name, xe_cstr value, uint len, uint flags){
	return queue_io(IORING_OP_FSETXATTR, req, [&](io_uring_sqe& sqe){
		xe_sqe_fxattr(sqe, sq_flags, fd, name, value, len, flags);
	});
}

int xe_loop::getxattr_ex(xe_req& req, byte sq_flags, xe_cstr path, xe_cstr name, char* value, uint len){
	return queue_io(IORING_OP_GETXATTR, req, [&](io_uring_sqe& sqe){
		xe_sqe_xattr(sqe, sq_flags, path, name, value, len, 0);
	});
}

int xe_loop::setxattr_ex(xe_req& req, byte sq_flags, xe_cstr path, xe_cstr name, xe_cstr value, uint len, uint flags){
	return queue_io(IORING_OP_SETXATTR, req, [&](io_uring_sqe& sqe){
		xe_sqe_xattr(sqe, sq_flags, path, name, value, len, flags);
	});
}

int xe_loop::splice_ex(xe_req& req, byte sq_flags, int fd_in, long off_in, int fd_out, long off_out, uint len, uint flags){
	return queue_io(IORING_OP_SPLICE, req, [&](io_uring_sqe& sqe){
		xe_sqe_splice(sqe, sq_flags, fd_in, off_in, fd_out, off_out, len, flags);
	});
}

int xe_loop::tee_ex(xe_req& req, byte sq_flags, int fd_in, int fd_out, uint len, uint flags){
	return queue_io(IORING_OP_TEE, req, [&](io_uring_sqe& sqe){
		xe_sqe_splice(sqe, sq_flags, fd_in, 0, fd_out, 0, len, flags);
	});
}

int xe_loop::statx_ex(xe_req& req, byte sq_flags, int fd, xe_cstr path, uint flags, uint mask, struct statx* statx){
	return queue_io(IORING_OP_STATX, req, [&](io_uring_sqe& sqe){
		xe_sqe_rw_fixed(sqe, sq_flags, 0, fd, path, mask, (ulong)statx, flags, 0);
	});
}

int xe_loop::socket_ex(xe_req& req, byte sq_flags, int af, int type, int protocol, uint flags, uint file_index){
	return queue_io(IORING_OP_SOCKET, req, [&](io_uring_sqe& sqe){
		xe_sqe_socket(sqe, sq_flags, af, null, protocol, type, flags, file_index);
	});
}

int xe_loop::connect_ex(xe_req& req, byte sq_flags, int fd, const sockaddr* addr, socklen_t addrlen){
	return queue_io(IORING_OP_CONNECT, req, [&](io_uring_sqe& sqe){
		xe_sqe_socket(sqe, sq_flags, fd, addr, 0, addrlen, 0, 0);
	});
}

int xe_loop::accept_ex(xe_req& req, byte sq_flags, ushort ioprio, int fd, sockaddr* addr, socklen_t* addrlen, uint flags, uint file_index){
	return queue_io(IORING_OP_ACCEPT, req, [&](io_uring_sqe& sqe){
		xe_sqe_socket(sqe, sq_flags, fd, addr, 0, (ulong)addrlen, flags, file_index);

		sqe.ioprio = ioprio;
	});
}

int xe_loop::recv_ex(xe_req& req, byte sq_flags, ushort ioprio, int fd, xe_ptr buf, uint len, uint flags){
	return queue_io(IORING_OP_RECV, req, [&](io_uring_sqe& sqe){
		xe_sqe_socket_rw(sqe, sq_flags, ioprio, fd, buf, len, flags);
	});
}

int xe_loop::send_ex(xe_req& req, byte sq_flags, ushort ioprio, int fd, xe_cptr buf, uint len, uint flags){
	return queue_io(IORING_OP_SEND, req, [&](io_uring_sqe& sqe){
		xe_sqe_socket_rw(sqe, sq_flags, ioprio, fd, buf, len, flags);
	});
}

int xe_loop::recvmsg_ex(xe_req& req, byte sq_flags, ushort ioprio, int fd, msghdr* msg, uint flags){
	return queue_io(IORING_OP_RECVMSG, req, [&](io_uring_sqe& sqe){
		xe_sqe_socket_rw(sqe, sq_flags, ioprio, fd, msg, 1, flags);
	});
}

int xe_loop::sendmsg_ex(xe_req& req, byte sq_flags, ushort ioprio, int fd, const msghdr* msg, uint flags){
	return queue_io(IORING_OP_SENDMSG, req, [&](io_uring_sqe& sqe){
		xe_sqe_socket_rw(sqe, sq_flags, ioprio, fd, msg, 1, flags);
	});
}

int xe_loop::send_zc_ex(xe_req& req, byte sq_flags, ushort ioprio, int fd, xe_cptr buf, uint len, uint flags, uint buf_index){
	return sendto_zc_ex(req, sq_flags, ioprio, fd, buf, len, flags, null, 0, buf_index);
}

int xe_loop::sendto_zc_ex(xe_req& req, byte sq_flags, ushort ioprio, int fd, xe_cptr buf, uint len, uint flags, const sockaddr* addr, socklen_t addrlen, uint buf_index){
	return queue_io(IORING_OP_SEND_ZC, req, [&](io_uring_sqe& sqe){
		xe_sqe_init(sqe, sq_flags, ioprio);

		sqe.fd = fd;
		sqe.addr = (ulong)buf;
		sqe.len = len;
		sqe.addr2 = (ulong)addr;
		sqe.addr_len = addrlen;
		sqe.msg_flags = flags;
		sqe.addr3 = 0;
		sqe.buf_index = buf_index;
		sqe.__pad2[0] = 0;
		sqe.__pad3[0] = 0;
	});
}

int xe_loop::shutdown_ex(xe_req& req, byte sq_flags, int fd, int how){
	return queue_io(IORING_OP_SHUTDOWN, req, [&](io_uring_sqe& sqe){
		xe_sqe_socket(sqe, sq_flags, fd, null, how, 0, 0, 0);
	});
}

static inline uint xe_poll_mask(uint mask){
	if constexpr(XE_BYTE_ORDER == XE_BIG_ENDIAN)
		mask = __swahw32(mask);
	return mask;
}

int xe_loop::poll_ex(xe_req& req, byte sq_flags, ushort ioprio, int fd, uint mask){
	return queue_io(IORING_OP_POLL_ADD, req, [&](io_uring_sqe& sqe){
		xe_sqe_rw_fixed(sqe, sq_flags, 0, fd, null, ioprio, 0, xe_poll_mask(mask), 0);
	});
}

int xe_loop::poll_update_ex(xe_req& req, byte sq_flags, xe_req& poll, uint mask, uint flags){
	return queue_io(IORING_OP_POLL_REMOVE, req, [&](io_uring_sqe& sqe){
		xe_sqe_init(sqe, sq_flags, 0);

		sqe.addr = (ulong)&poll;
		sqe.len = flags;
		sqe.off = 0;
		sqe.poll32_events = xe_poll_mask(mask);
		sqe.buf_index = 0;
		sqe.splice_fd_in = 0;
	});
}

int xe_loop::epoll_ctl_ex(xe_req& req, byte sq_flags, int epfd, int op, int fd, epoll_event* events){
	return queue_io(IORING_OP_EPOLL_CTL, req, [&](io_uring_sqe& sqe){
		xe_sqe_init(sqe, sq_flags, 0);

		sqe.buf_index = 0;
		sqe.splice_fd_in = 0;
		sqe.fd = epfd;
		sqe.addr = (ulong)events;
		sqe.len = op;
		sqe.off = fd;
	});
}

int xe_loop::poll_cancel_ex(xe_req& req, byte sq_flags, xe_req& cancel){
	return queue_cancel(IORING_OP_POLL_REMOVE, req, cancel, [&](io_uring_sqe& sqe){
		xe_sqe_init(sqe, sq_flags, 0);

		sqe.len = 0;
		sqe.off = 0;
		sqe.poll32_events = 0;
		sqe.buf_index = 0;
		sqe.splice_fd_in = 0;
	});
}

int xe_loop::cancel_ex(xe_req& req, byte sq_flags, xe_req& cancel, uint flags){
	return queue_cancel(IORING_OP_ASYNC_CANCEL, req, cancel, [&](io_uring_sqe& sqe){
		xe_sqe_init(sqe, sq_flags, 0);

		sqe.len = 0;
		sqe.off = 0;
		sqe.cancel_flags = flags;
		sqe.splice_fd_in = 0;
	});
}

int xe_loop::cancel_ex(xe_req& req, byte sq_flags, int fd, uint flags){
	return queue_io(IORING_OP_ASYNC_CANCEL, req, [&](io_uring_sqe& sqe){
		xe_sqe_rw(sqe, sq_flags, 0, fd, null, 0, 0, flags | IORING_ASYNC_CANCEL_FD);

		sqe.splice_fd_in = 0;
	});
}

int xe_loop::cancel_fixed_ex(xe_req& req, byte sq_flags, uint file_index, uint flags){
	/* safe to cast file_index to int here */
	return cancel_ex(req, sq_flags, file_index, flags | IORING_ASYNC_CANCEL_FD_FIXED);
}

int xe_loop::cancel_all_ex(xe_req& req, byte sq_flags){
	return queue_io(IORING_OP_ASYNC_CANCEL, req, [&](io_uring_sqe& sqe){
		xe_sqe_rw(sqe, sq_flags, 0, 0, null, 0, 0, IORING_ASYNC_CANCEL_ANY);

		sqe.splice_fd_in = 0;
	});
}

int xe_loop::files_update_ex(xe_req& req, byte sq_flags, int* fds, uint len, uint offset){
	return queue_io(IORING_OP_FILES_UPDATE, req, [&](io_uring_sqe& sqe){
		xe_sqe_init(sqe, sq_flags, 0);

		sqe.addr = (ulong)fds;
		sqe.len = len;
		sqe.off = offset;
		sqe.rw_flags = 0;
		sqe.splice_fd_in = 0;
	});
}

int xe_loop::provide_buffers_ex(xe_req& req, byte sq_flags, xe_ptr addr, uint len, ushort nr, ushort bgid, ushort bid){
	return queue_io(IORING_OP_PROVIDE_BUFFERS, req, [&](io_uring_sqe& sqe){
		xe_sqe_buffer(sqe, sq_flags, addr, len, nr, bgid, bid);
	});
}

int xe_loop::remove_buffers_ex(xe_req& req, byte sq_flags, ushort nr, ushort bgid){
	return queue_io(IORING_OP_REMOVE_BUFFERS, req, [&](io_uring_sqe& sqe){
		xe_sqe_buffer(sqe, sq_flags, null, 0, nr, bgid, 0);
	});
}

template<class F>
inline xe_promise xe_loop::make_promise(int ok_code, F start_promise){
	xe_promise promise;
	int res = start_promise(promise);

	if(res != ok_code){
		promise.result_ = res;
		promise.ready_ = true;
	}

	return promise;
}

xe_cstr xe_loop::class_name(){
	return "xe_loop";
}

#define XE_OP_ALIAS1(func, ...) int xe_loop::func(xe_req& req, ##__VA_ARGS__)
#define XE_OP_ALIAS1_BODY(func, ...) {			\
	return func##_ex(req, 0, ##__VA_ARGS__);	\
}

#define XE_OP_ALIAS1_PROMISE_EX(func, ...) xe_promise xe_loop::func##_ex(byte sq_flags, ##__VA_ARGS__)
#define XE_OP_ALIAS1_PROMISE_BODY_EX(func, ...) {		\
	return make_promise(0, [&](xe_promise& promise){	\
		return func##_ex(promise, sq_flags, ##__VA_ARGS__);	\
	});													\
}

#define XE_OP_ALIAS1_CANCEL_PROMISE_BODY_EX(func, ...) {	\
	return make_promise(XE_EINPROGRESS, [&](xe_promise& promise){	\
		return func##_ex(promise, sq_flags, ##__VA_ARGS__);	\
	});														\
}

#define XE_OP_ALIAS1_PROMISE(func, ...) xe_promise xe_loop::func(__VA_ARGS__)
#define XE_OP_ALIAS1_PROMISE_BODY(func, ...) {	\
	return func##_ex(0, ##__VA_ARGS__);			\
}

#define XE_OP_ALIAS2(func, ...) int xe_loop::func(xe_req& req, ##__VA_ARGS__)
#define XE_OP_ALIAS2_BODY(func, ...) {			\
	return func##_ex(req, 0, 0, ##__VA_ARGS__);	\
}

#define XE_OP_ALIAS2_PROMISE_EX(func, ...) xe_promise xe_loop::func##_ex(byte sq_flags, ushort ioprio, ##__VA_ARGS__)
#define XE_OP_ALIAS2_PROMISE_BODY_EX(func, ...) {		\
	return make_promise(0, [&](xe_promise& promise){	\
		return func##_ex(promise, sq_flags, ioprio, ##__VA_ARGS__);	\
	});													\
}

#define XE_OP_ALIAS2_PROMISE(func, ...) xe_promise xe_loop::func(__VA_ARGS__)
#define XE_OP_ALIAS2_PROMISE_BODY(func, ...) {	\
	return func##_ex(0, 0, ##__VA_ARGS__);		\
}

/* auto-generated by script, I could not figure out a better way to do this without templating */
XE_OP_ALIAS1(nop) XE_OP_ALIAS1_BODY(nop)
XE_OP_ALIAS1_PROMISE_EX(nop) XE_OP_ALIAS1_PROMISE_BODY_EX(nop)
XE_OP_ALIAS1_PROMISE(nop) XE_OP_ALIAS1_PROMISE_BODY(nop)

#define OPENAT_ARGS int dfd, xe_cstr path, uint flags, mode_t mode, uint file_index
#define OPENAT_VARS dfd, path, flags, mode, file_index
XE_OP_ALIAS1(openat, OPENAT_ARGS) XE_OP_ALIAS1_BODY(openat, OPENAT_VARS)
XE_OP_ALIAS1_PROMISE_EX(openat, OPENAT_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(openat, OPENAT_VARS)
XE_OP_ALIAS1_PROMISE(openat, OPENAT_ARGS) XE_OP_ALIAS1_PROMISE_BODY(openat, OPENAT_VARS)

#define OPENAT2_ARGS int dfd, xe_cstr path, open_how* how, uint file_index
#define OPENAT2_VARS dfd, path, how, file_index
XE_OP_ALIAS1(openat2, OPENAT2_ARGS) XE_OP_ALIAS1_BODY(openat2, OPENAT2_VARS)
XE_OP_ALIAS1_PROMISE_EX(openat2, OPENAT2_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(openat2, OPENAT2_VARS)
XE_OP_ALIAS1_PROMISE(openat2, OPENAT2_ARGS) XE_OP_ALIAS1_PROMISE_BODY(openat2, OPENAT2_VARS)

#define CLOSE_ARGS int fd
#define CLOSE_VARS fd
XE_OP_ALIAS1(close, CLOSE_ARGS) XE_OP_ALIAS1_BODY(close, CLOSE_VARS)
XE_OP_ALIAS1_PROMISE_EX(close, CLOSE_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(close, CLOSE_VARS)
XE_OP_ALIAS1_PROMISE(close, CLOSE_ARGS) XE_OP_ALIAS1_PROMISE_BODY(close, CLOSE_VARS)

#define CLOSE_DIRECT_ARGS uint file_index
#define CLOSE_DIRECT_VARS file_index
XE_OP_ALIAS1(close_direct, CLOSE_DIRECT_ARGS) XE_OP_ALIAS1_BODY(close_direct, CLOSE_DIRECT_VARS)
XE_OP_ALIAS1_PROMISE_EX(close_direct, CLOSE_DIRECT_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(close_direct, CLOSE_DIRECT_VARS)
XE_OP_ALIAS1_PROMISE(close_direct, CLOSE_DIRECT_ARGS) XE_OP_ALIAS1_PROMISE_BODY(close_direct, CLOSE_DIRECT_VARS)

#define READ_ARGS int fd, xe_ptr buf, uint len, long offset, uint flags
#define READ_VARS fd, buf, len, offset, flags
XE_OP_ALIAS2(read, READ_ARGS) XE_OP_ALIAS2_BODY(read, READ_VARS)
XE_OP_ALIAS2_PROMISE_EX(read, READ_ARGS) XE_OP_ALIAS2_PROMISE_BODY_EX(read, READ_VARS)
XE_OP_ALIAS2_PROMISE(read, READ_ARGS) XE_OP_ALIAS2_PROMISE_BODY(read, READ_VARS)

#define WRITE_ARGS int fd, xe_cptr buf, uint len, long offset, uint flags
#define WRITE_VARS fd, buf, len, offset, flags
XE_OP_ALIAS2(write, WRITE_ARGS) XE_OP_ALIAS2_BODY(write, WRITE_VARS)
XE_OP_ALIAS2_PROMISE_EX(write, WRITE_ARGS) XE_OP_ALIAS2_PROMISE_BODY_EX(write, WRITE_VARS)
XE_OP_ALIAS2_PROMISE(write, WRITE_ARGS) XE_OP_ALIAS2_PROMISE_BODY(write, WRITE_VARS)

#define READV_ARGS int fd, const iovec* iovecs, uint vlen, long offset, uint flags
#define READV_VARS fd, iovecs, vlen, offset, flags
XE_OP_ALIAS2(readv, READV_ARGS) XE_OP_ALIAS2_BODY(readv, READV_VARS)
XE_OP_ALIAS2_PROMISE_EX(readv, READV_ARGS) XE_OP_ALIAS2_PROMISE_BODY_EX(readv, READV_VARS)
XE_OP_ALIAS2_PROMISE(readv, READV_ARGS) XE_OP_ALIAS2_PROMISE_BODY(readv, READV_VARS)

#define WRITEV_ARGS int fd, const iovec* iovecs, uint vlen, long offset, uint flags
#define WRITEV_VARS fd, iovecs, vlen, offset, flags
XE_OP_ALIAS2(writev, WRITEV_ARGS) XE_OP_ALIAS2_BODY(writev, WRITEV_VARS)
XE_OP_ALIAS2_PROMISE_EX(writev, WRITEV_ARGS) XE_OP_ALIAS2_PROMISE_BODY_EX(writev, WRITEV_VARS)
XE_OP_ALIAS2_PROMISE(writev, WRITEV_ARGS) XE_OP_ALIAS2_PROMISE_BODY(writev, WRITEV_VARS)

#define READ_FIXED_ARGS int fd, xe_ptr buf, uint len, long offset, uint buf_index, uint flags
#define READ_FIXED_VARS fd, buf, len, offset, buf_index, flags
XE_OP_ALIAS2(read_fixed, READ_FIXED_ARGS) XE_OP_ALIAS2_BODY(read_fixed, READ_FIXED_VARS)
XE_OP_ALIAS2_PROMISE_EX(read_fixed, READ_FIXED_ARGS) XE_OP_ALIAS2_PROMISE_BODY_EX(read_fixed, READ_FIXED_VARS)
XE_OP_ALIAS2_PROMISE(read_fixed, READ_FIXED_ARGS) XE_OP_ALIAS2_PROMISE_BODY(read_fixed, READ_FIXED_VARS)

#define WRITE_FIXED_ARGS int fd, xe_cptr buf, uint len, long offset, uint buf_index, uint flags
#define WRITE_FIXED_VARS fd, buf, len, offset, buf_index, flags
XE_OP_ALIAS2(write_fixed, WRITE_FIXED_ARGS) XE_OP_ALIAS2_BODY(write_fixed, WRITE_FIXED_VARS)
XE_OP_ALIAS2_PROMISE_EX(write_fixed, WRITE_FIXED_ARGS) XE_OP_ALIAS2_PROMISE_BODY_EX(write_fixed, WRITE_FIXED_VARS)
XE_OP_ALIAS2_PROMISE(write_fixed, WRITE_FIXED_ARGS) XE_OP_ALIAS2_PROMISE_BODY(write_fixed, WRITE_FIXED_VARS)

#define FSYNC_ARGS int fd, uint flags
#define FSYNC_VARS fd, flags
XE_OP_ALIAS1(fsync, FSYNC_ARGS) XE_OP_ALIAS1_BODY(fsync, FSYNC_VARS)
XE_OP_ALIAS1_PROMISE_EX(fsync, FSYNC_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(fsync, FSYNC_VARS)
XE_OP_ALIAS1_PROMISE(fsync, FSYNC_ARGS) XE_OP_ALIAS1_PROMISE_BODY(fsync, FSYNC_VARS)

#define SYNC_FILE_RANGE_ARGS int fd, uint len, long offset, uint flags
#define SYNC_FILE_RANGE_VARS fd, len, offset, flags
XE_OP_ALIAS1(sync_file_range, SYNC_FILE_RANGE_ARGS) XE_OP_ALIAS1_BODY(sync_file_range, SYNC_FILE_RANGE_VARS)
XE_OP_ALIAS1_PROMISE_EX(sync_file_range, SYNC_FILE_RANGE_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(sync_file_range, SYNC_FILE_RANGE_VARS)
XE_OP_ALIAS1_PROMISE(sync_file_range, SYNC_FILE_RANGE_ARGS) XE_OP_ALIAS1_PROMISE_BODY(sync_file_range, SYNC_FILE_RANGE_VARS)

#define FALLOCATE_ARGS int fd, int mode, long offset, long len
#define FALLOCATE_VARS fd, mode, offset, len
XE_OP_ALIAS1(fallocate, FALLOCATE_ARGS) XE_OP_ALIAS1_BODY(fallocate, FALLOCATE_VARS)
XE_OP_ALIAS1_PROMISE_EX(fallocate, FALLOCATE_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(fallocate, FALLOCATE_VARS)
XE_OP_ALIAS1_PROMISE(fallocate, FALLOCATE_ARGS) XE_OP_ALIAS1_PROMISE_BODY(fallocate, FALLOCATE_VARS)

#define FADVISE_ARGS int fd, ulong offset, uint len, uint advice
#define FADVISE_VARS fd, offset, len, advice
XE_OP_ALIAS1(fadvise, FADVISE_ARGS) XE_OP_ALIAS1_BODY(fadvise, FADVISE_VARS)
XE_OP_ALIAS1_PROMISE_EX(fadvise, FADVISE_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(fadvise, FADVISE_VARS)
XE_OP_ALIAS1_PROMISE(fadvise, FADVISE_ARGS) XE_OP_ALIAS1_PROMISE_BODY(fadvise, FADVISE_VARS)

#define MADVISE_ARGS xe_ptr addr, uint len, uint advice
#define MADVISE_VARS addr, len, advice
XE_OP_ALIAS1(madvise, MADVISE_ARGS) XE_OP_ALIAS1_BODY(madvise, MADVISE_VARS)
XE_OP_ALIAS1_PROMISE_EX(madvise, MADVISE_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(madvise, MADVISE_VARS)
XE_OP_ALIAS1_PROMISE(madvise, MADVISE_ARGS) XE_OP_ALIAS1_PROMISE_BODY(madvise, MADVISE_VARS)

#define RENAMEAT_ARGS int old_dfd, xe_cstr old_path, int new_dfd, xe_cstr new_path, uint flags
#define RENAMEAT_VARS old_dfd, old_path, new_dfd, new_path, flags
XE_OP_ALIAS1(renameat, RENAMEAT_ARGS) XE_OP_ALIAS1_BODY(renameat, RENAMEAT_VARS)
XE_OP_ALIAS1_PROMISE_EX(renameat, RENAMEAT_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(renameat, RENAMEAT_VARS)
XE_OP_ALIAS1_PROMISE(renameat, RENAMEAT_ARGS) XE_OP_ALIAS1_PROMISE_BODY(renameat, RENAMEAT_VARS)

#define UNLINKAT_ARGS int dfd, xe_cstr path, uint flags
#define UNLINKAT_VARS dfd, path, flags
XE_OP_ALIAS1(unlinkat, UNLINKAT_ARGS) XE_OP_ALIAS1_BODY(unlinkat, UNLINKAT_VARS)
XE_OP_ALIAS1_PROMISE_EX(unlinkat, UNLINKAT_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(unlinkat, UNLINKAT_VARS)
XE_OP_ALIAS1_PROMISE(unlinkat, UNLINKAT_ARGS) XE_OP_ALIAS1_PROMISE_BODY(unlinkat, UNLINKAT_VARS)

#define MKDIRAT_ARGS int dfd, xe_cstr path, mode_t mode
#define MKDIRAT_VARS dfd, path, mode
XE_OP_ALIAS1(mkdirat, MKDIRAT_ARGS) XE_OP_ALIAS1_BODY(mkdirat, MKDIRAT_VARS)
XE_OP_ALIAS1_PROMISE_EX(mkdirat, MKDIRAT_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(mkdirat, MKDIRAT_VARS)
XE_OP_ALIAS1_PROMISE(mkdirat, MKDIRAT_ARGS) XE_OP_ALIAS1_PROMISE_BODY(mkdirat, MKDIRAT_VARS)

#define SYMLINKAT_ARGS xe_cstr target, int newdirfd, xe_cstr linkpath
#define SYMLINKAT_VARS target, newdirfd, linkpath
XE_OP_ALIAS1(symlinkat, SYMLINKAT_ARGS) XE_OP_ALIAS1_BODY(symlinkat, SYMLINKAT_VARS)
XE_OP_ALIAS1_PROMISE_EX(symlinkat, SYMLINKAT_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(symlinkat, SYMLINKAT_VARS)
XE_OP_ALIAS1_PROMISE(symlinkat, SYMLINKAT_ARGS) XE_OP_ALIAS1_PROMISE_BODY(symlinkat, SYMLINKAT_VARS)

#define LINKAT_ARGS int old_dfd, xe_cstr old_path, int new_dfd, xe_cstr new_path, uint flags
#define LINKAT_VARS old_dfd, old_path, new_dfd, new_path, flags
XE_OP_ALIAS1(linkat, LINKAT_ARGS) XE_OP_ALIAS1_BODY(linkat, LINKAT_VARS)
XE_OP_ALIAS1_PROMISE_EX(linkat, LINKAT_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(linkat, LINKAT_VARS)
XE_OP_ALIAS1_PROMISE(linkat, LINKAT_ARGS) XE_OP_ALIAS1_PROMISE_BODY(linkat, LINKAT_VARS)

#define FGETXATTR_ARGS int fd, xe_cstr name, char* value, uint len
#define FGETXATTR_VARS fd, name, value, len
XE_OP_ALIAS1(fgetxattr, FGETXATTR_ARGS) XE_OP_ALIAS1_BODY(fgetxattr, FGETXATTR_VARS)
XE_OP_ALIAS1_PROMISE_EX(fgetxattr, FGETXATTR_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(fgetxattr, FGETXATTR_VARS)
XE_OP_ALIAS1_PROMISE(fgetxattr, FGETXATTR_ARGS) XE_OP_ALIAS1_PROMISE_BODY(fgetxattr, FGETXATTR_VARS)

#define FSETXATTR_ARGS int fd, xe_cstr name, xe_cstr value, uint len, uint flags
#define FSETXATTR_VARS fd, name, value, len, flags
XE_OP_ALIAS1(fsetxattr, FSETXATTR_ARGS) XE_OP_ALIAS1_BODY(fsetxattr, FSETXATTR_VARS)
XE_OP_ALIAS1_PROMISE_EX(fsetxattr, FSETXATTR_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(fsetxattr, FSETXATTR_VARS)
XE_OP_ALIAS1_PROMISE(fsetxattr, FSETXATTR_ARGS) XE_OP_ALIAS1_PROMISE_BODY(fsetxattr, FSETXATTR_VARS)

#define GETXATTR_ARGS xe_cstr path, xe_cstr name, char* value, uint len
#define GETXATTR_VARS path, name, value, len
XE_OP_ALIAS1(getxattr, GETXATTR_ARGS) XE_OP_ALIAS1_BODY(getxattr, GETXATTR_VARS)
XE_OP_ALIAS1_PROMISE_EX(getxattr, GETXATTR_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(getxattr, GETXATTR_VARS)
XE_OP_ALIAS1_PROMISE(getxattr, GETXATTR_ARGS) XE_OP_ALIAS1_PROMISE_BODY(getxattr, GETXATTR_VARS)

#define SETXATTR_ARGS xe_cstr path, xe_cstr name, xe_cstr value, uint len, uint flags
#define SETXATTR_VARS path, name, value, len, flags
XE_OP_ALIAS1(setxattr, SETXATTR_ARGS) XE_OP_ALIAS1_BODY(setxattr, SETXATTR_VARS)
XE_OP_ALIAS1_PROMISE_EX(setxattr, SETXATTR_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(setxattr, SETXATTR_VARS)
XE_OP_ALIAS1_PROMISE(setxattr, SETXATTR_ARGS) XE_OP_ALIAS1_PROMISE_BODY(setxattr, SETXATTR_VARS)

#define SPLICE_ARGS int fd_in, long off_in, int fd_out, long off_out, uint len, uint flags
#define SPLICE_VARS fd_in, off_in, fd_out, off_out, len, flags
XE_OP_ALIAS1(splice, SPLICE_ARGS) XE_OP_ALIAS1_BODY(splice, SPLICE_VARS)
XE_OP_ALIAS1_PROMISE_EX(splice, SPLICE_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(splice, SPLICE_VARS)
XE_OP_ALIAS1_PROMISE(splice, SPLICE_ARGS) XE_OP_ALIAS1_PROMISE_BODY(splice, SPLICE_VARS)

#define TEE_ARGS int fd_in, int fd_out, uint len, uint flags
#define TEE_VARS fd_in, fd_out, len, flags
XE_OP_ALIAS1(tee, TEE_ARGS) XE_OP_ALIAS1_BODY(tee, TEE_VARS)
XE_OP_ALIAS1_PROMISE_EX(tee, TEE_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(tee, TEE_VARS)
XE_OP_ALIAS1_PROMISE(tee, TEE_ARGS) XE_OP_ALIAS1_PROMISE_BODY(tee, TEE_VARS)

#define STATX_ARGS int fd, xe_cstr path, uint flags, uint mask, struct statx* statx
#define STATX_VARS fd, path, flags, mask, statx
XE_OP_ALIAS1(statx, STATX_ARGS) XE_OP_ALIAS1_BODY(statx, STATX_VARS)
XE_OP_ALIAS1_PROMISE_EX(statx, STATX_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(statx, STATX_VARS)
XE_OP_ALIAS1_PROMISE(statx, STATX_ARGS) XE_OP_ALIAS1_PROMISE_BODY(statx, STATX_VARS)

#define SOCKET_ARGS int af, int type, int protocol, uint flags, uint file_index
#define SOCKET_VARS af, type, protocol, flags, file_index
XE_OP_ALIAS1(socket, SOCKET_ARGS) XE_OP_ALIAS1_BODY(socket, SOCKET_VARS)
XE_OP_ALIAS1_PROMISE_EX(socket, SOCKET_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(socket, SOCKET_VARS)
XE_OP_ALIAS1_PROMISE(socket, SOCKET_ARGS) XE_OP_ALIAS1_PROMISE_BODY(socket, SOCKET_VARS)

#define CONNECT_ARGS int fd, const sockaddr* addr, socklen_t addrlen
#define CONNECT_VARS fd, addr, addrlen
XE_OP_ALIAS1(connect, CONNECT_ARGS) XE_OP_ALIAS1_BODY(connect, CONNECT_VARS)
XE_OP_ALIAS1_PROMISE_EX(connect, CONNECT_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(connect, CONNECT_VARS)
XE_OP_ALIAS1_PROMISE(connect, CONNECT_ARGS) XE_OP_ALIAS1_PROMISE_BODY(connect, CONNECT_VARS)

#define ACCEPT_ARGS int fd, sockaddr* addr, socklen_t* addrlen, uint flags, uint file_index
#define ACCEPT_VARS fd, addr, addrlen, flags, file_index
XE_OP_ALIAS2(accept, ACCEPT_ARGS) XE_OP_ALIAS2_BODY(accept, ACCEPT_VARS)
XE_OP_ALIAS2_PROMISE_EX(accept, ACCEPT_ARGS) XE_OP_ALIAS2_PROMISE_BODY_EX(accept, ACCEPT_VARS)
XE_OP_ALIAS2_PROMISE(accept, ACCEPT_ARGS) XE_OP_ALIAS2_PROMISE_BODY(accept, ACCEPT_VARS)

#define RECV_ARGS int fd, xe_ptr buf, uint len, uint flags
#define RECV_VARS fd, buf, len, flags
XE_OP_ALIAS2(recv, RECV_ARGS) XE_OP_ALIAS2_BODY(recv, RECV_VARS)
XE_OP_ALIAS2_PROMISE_EX(recv, RECV_ARGS) XE_OP_ALIAS2_PROMISE_BODY_EX(recv, RECV_VARS)
XE_OP_ALIAS2_PROMISE(recv, RECV_ARGS) XE_OP_ALIAS2_PROMISE_BODY(recv, RECV_VARS)

#define SEND_ARGS int fd, xe_cptr buf, uint len, uint flags
#define SEND_VARS fd, buf, len, flags
XE_OP_ALIAS2(send, SEND_ARGS) XE_OP_ALIAS2_BODY(send, SEND_VARS)
XE_OP_ALIAS2_PROMISE_EX(send, SEND_ARGS) XE_OP_ALIAS2_PROMISE_BODY_EX(send, SEND_VARS)
XE_OP_ALIAS2_PROMISE(send, SEND_ARGS) XE_OP_ALIAS2_PROMISE_BODY(send, SEND_VARS)

#define RECVMSG_ARGS int fd, msghdr* msg, uint flags
#define RECVMSG_VARS fd, msg, flags
XE_OP_ALIAS2(recvmsg, RECVMSG_ARGS) XE_OP_ALIAS2_BODY(recvmsg, RECVMSG_VARS)
XE_OP_ALIAS2_PROMISE_EX(recvmsg, RECVMSG_ARGS) XE_OP_ALIAS2_PROMISE_BODY_EX(recvmsg, RECVMSG_VARS)
XE_OP_ALIAS2_PROMISE(recvmsg, RECVMSG_ARGS) XE_OP_ALIAS2_PROMISE_BODY(recvmsg, RECVMSG_VARS)

#define SENDMSG_ARGS int fd, const msghdr* msg, uint flags
#define SENDMSG_VARS fd, msg, flags
XE_OP_ALIAS2(sendmsg, SENDMSG_ARGS) XE_OP_ALIAS2_BODY(sendmsg, SENDMSG_VARS)
XE_OP_ALIAS2_PROMISE_EX(sendmsg, SENDMSG_ARGS) XE_OP_ALIAS2_PROMISE_BODY_EX(sendmsg, SENDMSG_VARS)
XE_OP_ALIAS2_PROMISE(sendmsg, SENDMSG_ARGS) XE_OP_ALIAS2_PROMISE_BODY(sendmsg, SENDMSG_VARS)

#define SEND_ZC_ARGS int fd, xe_cptr buf, uint len, uint flags, uint buf_index
#define SEND_ZC_VARS fd, buf, len, flags, buf_index
XE_OP_ALIAS2(send_zc, SEND_ZC_ARGS) XE_OP_ALIAS2_BODY(send_zc, SEND_ZC_VARS)
XE_OP_ALIAS2_PROMISE_EX(send_zc, SEND_ZC_ARGS) XE_OP_ALIAS2_PROMISE_BODY_EX(send_zc, SEND_ZC_VARS)
XE_OP_ALIAS2_PROMISE(send_zc, SEND_ZC_ARGS) XE_OP_ALIAS2_PROMISE_BODY(send_zc, SEND_ZC_VARS)

#define SENDTO_ZC_ARGS int fd, xe_cptr buf, uint len, uint flags, const sockaddr* addr, socklen_t addrlen, uint buf_index
#define SENDTO_ZC_VARS fd, buf, len, flags, addr, addrlen, buf_index
XE_OP_ALIAS2(sendto_zc, SENDTO_ZC_ARGS) XE_OP_ALIAS2_BODY(sendto_zc, SENDTO_ZC_VARS)
XE_OP_ALIAS2_PROMISE_EX(sendto_zc, SENDTO_ZC_ARGS) XE_OP_ALIAS2_PROMISE_BODY_EX(sendto_zc, SENDTO_ZC_VARS)
XE_OP_ALIAS2_PROMISE(sendto_zc, SENDTO_ZC_ARGS) XE_OP_ALIAS2_PROMISE_BODY(sendto_zc, SENDTO_ZC_VARS)

#define SHUTDOWN_ARGS int fd, int how
#define SHUTDOWN_VARS fd, how
XE_OP_ALIAS1(shutdown, SHUTDOWN_ARGS) XE_OP_ALIAS1_BODY(shutdown, SHUTDOWN_VARS)
XE_OP_ALIAS1_PROMISE_EX(shutdown, SHUTDOWN_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(shutdown, SHUTDOWN_VARS)
XE_OP_ALIAS1_PROMISE(shutdown, SHUTDOWN_ARGS) XE_OP_ALIAS1_PROMISE_BODY(shutdown, SHUTDOWN_VARS)

#define POLL_ARGS int fd, uint mask
#define POLL_VARS fd, mask
XE_OP_ALIAS2(poll, POLL_ARGS) XE_OP_ALIAS2_BODY(poll, POLL_VARS)
XE_OP_ALIAS2_PROMISE_EX(poll, POLL_ARGS) XE_OP_ALIAS2_PROMISE_BODY_EX(poll, POLL_VARS)
XE_OP_ALIAS2_PROMISE(poll, POLL_ARGS) XE_OP_ALIAS2_PROMISE_BODY(poll, POLL_VARS)

#define POLL_UPDATE_ARGS xe_req& poll, uint mask, uint flags
#define POLL_UPDATE_VARS poll, mask, flags
XE_OP_ALIAS1(poll_update, POLL_UPDATE_ARGS) XE_OP_ALIAS1_BODY(poll_update, POLL_UPDATE_VARS)
XE_OP_ALIAS1_PROMISE_EX(poll_update, POLL_UPDATE_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(poll_update, POLL_UPDATE_VARS)
XE_OP_ALIAS1_PROMISE(poll_update, POLL_UPDATE_ARGS) XE_OP_ALIAS1_PROMISE_BODY(poll_update, POLL_UPDATE_VARS)

#define EPOLL_CTL_ARGS int epfd, int op, int fd, epoll_event* events
#define EPOLL_CTL_VARS epfd, op, fd, events
XE_OP_ALIAS1(epoll_ctl, EPOLL_CTL_ARGS) XE_OP_ALIAS1_BODY(epoll_ctl, EPOLL_CTL_VARS)
XE_OP_ALIAS1_PROMISE_EX(epoll_ctl, EPOLL_CTL_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(epoll_ctl, EPOLL_CTL_VARS)
XE_OP_ALIAS1_PROMISE(epoll_ctl, EPOLL_CTL_ARGS) XE_OP_ALIAS1_PROMISE_BODY(epoll_ctl, EPOLL_CTL_VARS)

#define POLL_CANCEL_ARGS xe_req& cancel
#define POLL_CANCEL_VARS cancel
XE_OP_ALIAS1(poll_cancel, POLL_CANCEL_ARGS) XE_OP_ALIAS1_BODY(poll_cancel, POLL_CANCEL_VARS)
XE_OP_ALIAS1_PROMISE_EX(poll_cancel, POLL_CANCEL_ARGS) XE_OP_ALIAS1_CANCEL_PROMISE_BODY_EX(poll_cancel, POLL_CANCEL_VARS)
XE_OP_ALIAS1_PROMISE(poll_cancel, POLL_CANCEL_ARGS) XE_OP_ALIAS1_PROMISE_BODY(poll_cancel, POLL_CANCEL_VARS)

#define CANCEL_ARGS xe_req& cancel, uint flags
#define CANCEL_VARS cancel, flags
XE_OP_ALIAS1(cancel, CANCEL_ARGS) XE_OP_ALIAS1_BODY(cancel, CANCEL_VARS)
XE_OP_ALIAS1_PROMISE_EX(cancel, CANCEL_ARGS) XE_OP_ALIAS1_CANCEL_PROMISE_BODY_EX(cancel, CANCEL_VARS)
XE_OP_ALIAS1_PROMISE(cancel, CANCEL_ARGS) XE_OP_ALIAS1_PROMISE_BODY(cancel, CANCEL_VARS)

#undef CANCEL_ARGS
#undef CANCEL_VARS
#define CANCEL_ARGS int fd, uint flags
#define CANCEL_VARS fd, flags
XE_OP_ALIAS1(cancel, CANCEL_ARGS) XE_OP_ALIAS1_BODY(cancel, CANCEL_VARS)
XE_OP_ALIAS1_PROMISE_EX(cancel, CANCEL_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(cancel, CANCEL_VARS)
XE_OP_ALIAS1_PROMISE(cancel, CANCEL_ARGS) XE_OP_ALIAS1_PROMISE_BODY(cancel, CANCEL_VARS)

#define CANCEL_FIXED_ARGS uint file_index, uint flags
#define CANCEL_FIXED_VARS file_index, flags
XE_OP_ALIAS1(cancel_fixed, CANCEL_FIXED_ARGS) XE_OP_ALIAS1_BODY(cancel_fixed, CANCEL_FIXED_VARS)
XE_OP_ALIAS1_PROMISE_EX(cancel_fixed, CANCEL_FIXED_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(cancel_fixed, CANCEL_FIXED_VARS)
XE_OP_ALIAS1_PROMISE(cancel_fixed, CANCEL_FIXED_ARGS) XE_OP_ALIAS1_PROMISE_BODY(cancel_fixed, CANCEL_FIXED_VARS)

XE_OP_ALIAS1(cancel_all) XE_OP_ALIAS1_BODY(cancel_all)
XE_OP_ALIAS1_PROMISE_EX(cancel_all) XE_OP_ALIAS1_PROMISE_BODY_EX(cancel_all)
XE_OP_ALIAS1_PROMISE(cancel_all) XE_OP_ALIAS1_PROMISE_BODY(cancel_all)

#define FILES_UPDATE_ARGS int* fds, uint len, uint offset
#define FILES_UPDATE_VARS fds, len, offset
XE_OP_ALIAS1(files_update, FILES_UPDATE_ARGS) XE_OP_ALIAS1_BODY(files_update, FILES_UPDATE_VARS)
XE_OP_ALIAS1_PROMISE_EX(files_update, FILES_UPDATE_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(files_update, FILES_UPDATE_VARS)
XE_OP_ALIAS1_PROMISE(files_update, FILES_UPDATE_ARGS) XE_OP_ALIAS1_PROMISE_BODY(files_update, FILES_UPDATE_VARS)

#define PROVIDE_BUFFERS_ARGS xe_ptr addr, uint len, ushort nr, ushort bgid, ushort bid
#define PROVIDE_BUFFERS_VARS addr, len, nr, bgid, bid
XE_OP_ALIAS1(provide_buffers, PROVIDE_BUFFERS_ARGS) XE_OP_ALIAS1_BODY(provide_buffers, PROVIDE_BUFFERS_VARS)
XE_OP_ALIAS1_PROMISE_EX(provide_buffers, PROVIDE_BUFFERS_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(provide_buffers, PROVIDE_BUFFERS_VARS)
XE_OP_ALIAS1_PROMISE(provide_buffers, PROVIDE_BUFFERS_ARGS) XE_OP_ALIAS1_PROMISE_BODY(provide_buffers, PROVIDE_BUFFERS_VARS)

#define REMOVE_BUFFERS_ARGS ushort nr, ushort bgid
#define REMOVE_BUFFERS_VARS nr, bgid
XE_OP_ALIAS1(remove_buffers, REMOVE_BUFFERS_ARGS) XE_OP_ALIAS1_BODY(remove_buffers, REMOVE_BUFFERS_VARS)
XE_OP_ALIAS1_PROMISE_EX(remove_buffers, REMOVE_BUFFERS_ARGS) XE_OP_ALIAS1_PROMISE_BODY_EX(remove_buffers, REMOVE_BUFFERS_VARS)
XE_OP_ALIAS1_PROMISE(remove_buffers, REMOVE_BUFFERS_ARGS) XE_OP_ALIAS1_PROMISE_BODY(remove_buffers, REMOVE_BUFFERS_VARS)