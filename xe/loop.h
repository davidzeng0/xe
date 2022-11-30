#pragma once
#if !defined XE_COROUTINE_EXPERIMENTAL && defined __clang__ && __clang_major__ < 14
	#define XE_COROUTINE_EXPERIMENTAL 1
#endif

#if XE_COROUTINE_EXPERIMENTAL == 1
#include <experimental/coroutine>

typedef typename std::experimental::coroutine_handle<> xe_coroutine_handle;
#else
#include <coroutine>

typedef typename std::coroutine_handle<> xe_coroutine_handle;
#endif

#include <chrono>
#include <liburing.h>
#include "xstd/types.h"
#include "xstd/rbtree.h"
#include "xstd/linked_list.h"
#include "xutil/util.h"
#include "error.h"
#include "op.h"

enum xe_iobuf_size{
	XE_LOOP_IOBUF_SIZE = 16 * 1024,
	XE_LOOP_IOBUF_SIZE_LARGE = 256 * 1024
};

class xe_req_info : protected xe_linked_node{
private:
	xe_op op;

	friend class xe_loop;
public:
	xe_req_info() = default;

	xe_disable_copy_move(xe_req_info)

	~xe_req_info() = default;
};

class xe_req{
public:
	union{
		void (*event)(xe_req& req, int res, uint flags);
		void (*callback)(xe_req& req, int res);
	};

	xe_req(): event(){}
	~xe_req() = default;
};

enum xe_timer_flags{
	XE_TIMER_NONE = 0x0,
	XE_TIMER_REPEAT = 0x1,
	XE_TIMER_ABS = 0x2,
	XE_TIMER_ALIGN = 0x4, /* set next timeout to expire time + repeat instead of now + repeat */
	XE_TIMER_PASSIVE = 0x8 /* timer does not prevent loop from exiting */
};

class xe_timer : public xe_rb_node{
private:
	ulong expire;
	ulong delay;

	bool active_: 1;
	bool repeat_: 1;
	bool align_: 1;
	bool passive_: 1;
	bool in_callback: 1;

	friend class xe_loop;
public:
	int (*callback)(xe_loop& loop, xe_timer& timer);

	xe_timer(){
		expire = 0;
		delay = 0;

		active_ = false;
		repeat_ = false;
		align_ = false;
		passive_ = false;
		in_callback = false;

		callback = null;
	}

	xe_disable_copy_move(xe_timer)

	bool active() const{
		return active_;
	}

	bool repeat() const{
		return repeat_;
	}

	bool align() const{
		return align_;
	}

	bool passive() const{
		return passive_;
	}

	bool operator<(const xe_timer& o) const{
		return expire < o.expire;
	}

	bool operator>(const xe_timer& o) const{
		return expire > o.expire;
	}

	bool operator==(const xe_timer& o) const{
		return expire == o.expire;
	}

	~xe_timer() = default;
};

struct xe_loop_options{
	uint entries; /* number of sqes */
	uint cq_entries; /* number of cqes */
	uint sq_thread_cpu;
	uint wq_fd;

	bool flag_sqpoll: 1;
	bool flag_iopoll: 1;
	bool flag_sqaff: 1;
	bool flag_cqsize: 1;
	bool flag_attach_wq: 1;

	/* xe flags */
	bool flag_iobuf: 1; /* loop allocates a buffer for sync I/O */

	xe_loop_options(){
		entries = 0;
		cq_entries = 0;
		sq_thread_cpu = 0;
		wq_fd = 0;

		flag_sqpoll = false;
		flag_iopoll = false;
		flag_sqaff = false;
		flag_cqsize = false;
		flag_attach_wq = false;

		flag_iobuf = false;
	}

	~xe_loop_options() = default;
};

class xe_promise : public xe_req{
protected:
	static void complete(xe_req&, int, uint);

	xe_promise();
	xe_promise(xe_promise&&) = default;

	xe_disable_copy(xe_promise)
	xe_disable_move_assign(xe_promise)

	xe_coroutine_handle waiter;
	int result_;

	union{
		uint flags_;

		struct{
			uint cqe_flags: 15;
			uint ready_: 1;
			uint buffer_id_: 16;
		};
	};

	friend class xe_loop;
public:
	static xe_promise done(int result){
		xe_promise promise;

		promise.result_ = result;
		promise.ready_ = true;

		return promise;
	}

	bool await_ready() const{
		return ready_;
	}

	void await_suspend(xe_coroutine_handle handle){
		waiter = handle;
	}

	int await_resume(){
		if(more()){
			waiter = null;
			ready_ = false;
		}

		return result_;
	}

	bool ready() const{
		return ready_;
	}

	int result() const{
		return result_;
	}

	uint flags() const{
		return cqe_flags;
	}

	bool more() const{
		return cqe_flags & IORING_CQE_F_MORE;
	}

	ushort buffer_id(){
		return buffer_id_;
	}

	~xe_promise() = default;
};

class xe_loop{
private:
	io_uring ring;

	ulong active_timers;
	xe_rbtree<xe_timer> timers;

	xe_ptr io_buf;
	xe_linked_list<xe_req_info> reqs;

	ulong handles_;
	uint queued_;

	int error;
	bool sq_ring_full: 1;

	int submit(bool);

	void run_timer(xe_timer&, ulong);
	void queue_timer(xe_timer&);
	void erase_timer(xe_timer&);

	int queue_io(xe_req_info&);
	int queue_pending();

	int get_sqe(xe_req&, io_uring_sqe*&, xe_req_info* info);
public:
	xe_loop(){
		active_timers = 0;

		io_buf = null;

		handles_ = 0;
		queued_ = 0;

		error = 0;
		sq_ring_full = false;
	}

	xe_disable_copy_move(xe_loop)

	int init(uint entries);
	int init_options(xe_loop_options& options);
	void close();

	int run();

	xe_inline int run(xe_req& req, xe_op op, xe_req_info* info = null){
		io_uring_sqe* sqe;

		xe_return_error(get_sqe(req, sqe, info));

		/* copy io parameters */
		*sqe = op.sqe;
		sqe -> user_data = (ulong)&req;

		return 0;
	}

	xe_inline int cancel(xe_req& req, xe_req& cancel, xe_op op, xe_req_info* info = null, xe_req_info* cancel_info = null){
		if(cancel_info && cancel_info -> linked()){
			/* the request was put in our deferred queue */
			if(op.sqe.opcode == IORING_OP_POLL_REMOVE && op.sqe.len & (IORING_POLL_UPDATE_EVENTS | IORING_POLL_UPDATE_USER_DATA)){
				/* poll update, not queued yet so just modify here */
				if(op.sqe.len & IORING_POLL_UPDATE_EVENTS)
					cancel_info -> op.sqe.poll32_events = op.sqe.poll32_events;
				if(op.sqe.len & IORING_POLL_UPDATE_USER_DATA)
					cancel_info -> op.sqe.user_data = op.sqe.user_data;
			}else{
				/* finish cancel */
				cancel_info -> erase();
			}

			return 0;
		}

		op.sqe.addr = (ulong)&cancel;

		return run(req, op, info) ?: XE_EINPROGRESS;
	}

	xe_inline xe_promise run(xe_op op, xe_req_info* info = null){
		xe_promise promise;
		int res;

		res = run(promise, op, info);

		if(res != 0){
			promise.result_ = res;
			promise.ready_ = true;
		}

		return promise;
	}

	xe_inline xe_promise cancel(xe_req& cancel_req, xe_op op, xe_req_info* info = null, xe_req_info* cancel_info = null){
		xe_promise promise;
		int res;

		res = cancel(promise, cancel_req, op, info, cancel_info);

		if(res != XE_EINPROGRESS){
			promise.result_ = res;
			promise.ready_ = true;
		}

		return promise;
	}

	uint sqe_count() const; /* total sqes */
	uint cqe_count() const; /* total cqes */
	uint remain() const; /* remaining sqes */
	uint capacity() const; /* alias for sqe_count */

	uint queued() const; /* number of buffered requests */
	ulong handles() const; /* number of in progress requests */

	int flush(); /* submit any queued sqes */

	int timer_ms(xe_timer& timer, ulong time, ulong repeat, uint flags);
	int timer_ns(xe_timer& timer, ulong time, ulong repeat, uint flags);

	template<typename Rep0, class Period0, typename Rep1, class Period1>
	int timer(xe_timer& timer, std::chrono::duration<Rep0, Period0> time, std::chrono::duration<Rep1, Period1> repeat, uint flags){
		return timer_ns(timer,
			std::chrono::duration_cast<std::chrono::nanoseconds>(time).count(),
			std::chrono::duration_cast<std::chrono::nanoseconds>(repeat).count(),
			flags
		);
	}

	int cancel(xe_timer& timer);

	/* shared buffer for sync I/O */
	xe_ptr iobuf() const;
	xe_ptr iobuf_large() const;

	int register_buffers(const iovec* iov, uint vlen);
	int register_buffers_sparse(uint len);
	int register_buffers_update_tag(uint off, const iovec* iov, const ulong* tags, uint len);
	int unregister_buffers();

	int register_files(const int* fds, uint len);
	int register_files_tags(const int* fds, const ulong* tags, uint len);
	int register_files_sparse(uint len);
	int register_files_update(uint off, const int* fds, uint len);
	int register_files_update_tag(uint off, const int* fds, const ulong* tags, uint len);
	int register_file_alloc_range(uint off, uint len);
	int unregister_files();

	~xe_loop() = default;

	static xe_cstr class_name();
};