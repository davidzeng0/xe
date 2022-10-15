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
#include "xutil/util.h"

enum xe_iobuf_size{
	XE_LOOP_IOBUF_SIZE = 16 * 1024,
	XE_LOOP_IOBUF_SIZE_LARGE = 256 * 1024
};

class xe_req_info{
private:
	xe_req_info* prev;
	xe_req_info* next;
	io_uring_sqe sqe;

	friend class xe_loop;
public:
	xe_req_info(): prev(), next(){}

	xe_disallow_copy_move(xe_req_info)

	~xe_req_info() = default;
};

class xe_req{
public:
	union{
		void (*event)(xe_req& req, int res, uint flags);
		void (*callback)(xe_req& req, int res);
	};

	xe_req_info* info;

	xe_req(): event(), info(){}

	~xe_req() = default;
};

enum xe_timer_flags{
	XE_TIMER_NONE = 0x0,
	XE_TIMER_REPEAT = 0x1,
	XE_TIMER_ABS = 0x2,
	XE_TIMER_ALIGN = 0x4, /* set next timeout to expire time + repeat instead of now + repeat */
	XE_TIMER_PASSIVE = 0x8 /* timer does not prevent loop from exiting */
};

class xe_loop;
class xe_timer{
private:
	xe_rbtree<ulong>::node expire;
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
		expire.key = 0;
		delay = 0;

		active_ = false;
		repeat_ = false;
		align_ = false;
		passive_ = false;
		in_callback = false;

		callback = null;
	}

	xe_disallow_copy_move(xe_timer)

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

	xe_disallow_copy(xe_promise)
	xe_disallow_move_assign(xe_promise)

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
	xe_rbtree<ulong> timers;

	xe_ptr io_buf;
	xe_req_info* reqs;

	ulong handles;
	uint queued;

	int error;
	bool sq_ring_full: 1;

	int submit(uint, ulong, bool);
	int wait(uint, ulong);

	void run_timer(xe_timer&, ulong);
	void queue_timer(xe_timer&);
	void erase_timer(xe_timer&);

	template<class F>
	int queue_io(int, xe_req&, F);
	int queue_io(xe_req_info&);

	template<class F>
	int queue_cancel(int, xe_req&, xe_req&, F);

	int queue_pending();

	template<class F>
	xe_promise make_promise(int, F);
public:
	xe_loop(){
		active_timers = 0;

		io_buf = null;
		reqs = null;

		handles = 0;
		queued = 0;

		error = 0;
		sq_ring_full = false;
	}

	xe_disallow_copy_move(xe_loop)

	int init(uint entries);
	int init_options(xe_loop_options& options);
	void close();

	int run();

	uint sqe_count() const; /* total sqes */
	uint cqe_count() const; /* total cqes */
	uint remain() const; /* remaining sqes */
	uint capacity() const; /* alias for sqe_count */

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

	/* regular op */
#define XE_OP_DEFINE1(func, ...)			\
	int func(xe_req& req, ##__VA_ARGS__);	\
	int func##_ex(xe_req& req, byte sq_flags, ##__VA_ARGS__); \
	xe_promise func(__VA_ARGS__);			\
	xe_promise func##_ex(byte sq_flags, ##__VA_ARGS__);

	/* supports ioprio */
#define XE_OP_DEFINE2(func, ...)			\
	int func(xe_req& req, ##__VA_ARGS__);	\
	int func##_ex(xe_req& req, byte sq_flags, ushort ioprio, ##__VA_ARGS__); \
	xe_promise func(__VA_ARGS__);			\
	xe_promise func##_ex(byte sq_flags, ushort ioprio, ##__VA_ARGS__);

	XE_OP_DEFINE1(nop)

	XE_OP_DEFINE1(openat, 			int dfd, xe_cstr path, uint flags, mode_t mode, uint file_index = 0)
	XE_OP_DEFINE1(openat2, 			int dfd, xe_cstr path, open_how* how, uint file_index = 0)

	XE_OP_DEFINE1(close, 			int fd)
	XE_OP_DEFINE1(close_direct, 	uint file_index)

	XE_OP_DEFINE2(read, 			int fd, xe_ptr buf, uint len, long offset, uint flags = 0)
	XE_OP_DEFINE2(write, 			int fd, xe_cptr buf, uint len, long offset, uint flags = 0)
	XE_OP_DEFINE2(readv, 			int fd, const iovec* iovecs, uint vlen, long offset, uint flags = 0)
	XE_OP_DEFINE2(writev, 			int fd, const iovec* iovecs, uint vlen, long offset, uint flags = 0)
	XE_OP_DEFINE2(read_fixed, 		int fd, xe_ptr buf, uint len, long offset, uint buf_index, uint flags = 0)
	XE_OP_DEFINE2(write_fixed, 		int fd, xe_cptr buf, uint len, long offset, uint buf_index, uint flags = 0)

	XE_OP_DEFINE1(fsync, 			int fd, uint flags)
	XE_OP_DEFINE1(sync_file_range, 	int fd, uint len, long offset, uint flags)
	XE_OP_DEFINE1(fallocate, 		int fd, int mode, long offset, long len)

	XE_OP_DEFINE1(fadvise, 			int fd, ulong offset, uint len, uint advice)
	XE_OP_DEFINE1(madvise, 			xe_ptr addr, uint len, uint advice)

	XE_OP_DEFINE1(renameat, 		int old_dfd, xe_cstr old_path, int new_dfd, xe_cstr new_path, uint flags)
	XE_OP_DEFINE1(unlinkat, 		int dfd, xe_cstr path, uint flags)
	XE_OP_DEFINE1(mkdirat, 			int dfd, xe_cstr path, mode_t mode)
	XE_OP_DEFINE1(symlinkat, 		xe_cstr target, int newdirfd, xe_cstr linkpath)
	XE_OP_DEFINE1(linkat, 			int old_dfd, xe_cstr old_path, int new_dfd, xe_cstr new_path, uint flags)

	XE_OP_DEFINE1(fgetxattr, 		int fd, xe_cstr name, char* value, uint len)
	XE_OP_DEFINE1(fsetxattr, 		int fd, xe_cstr name, xe_cstr value, uint len, uint flags)
	XE_OP_DEFINE1(getxattr, 		xe_cstr path, xe_cstr name, char* value, uint len)
	XE_OP_DEFINE1(setxattr, 		xe_cstr path, xe_cstr name, xe_cstr value, uint len, uint flags)

	XE_OP_DEFINE1(splice, 			int fd_in, long off_in, int fd_out, long off_out, uint len, uint flags)
	XE_OP_DEFINE1(tee, 				int fd_in, int fd_out, uint len, uint flags)

	XE_OP_DEFINE1(statx, 			int fd, xe_cstr path, uint flags, uint mask, struct statx* statx)

	XE_OP_DEFINE1(socket, 			int af, int type, int protocol, uint flags, uint file_index = 0)

	XE_OP_DEFINE1(connect, 			int fd, const sockaddr* addr, socklen_t addrlen)
	XE_OP_DEFINE2(accept, 			int fd, sockaddr* addr, socklen_t* addrlen, uint flags, uint file_index = 0)

	XE_OP_DEFINE2(recv, 			int fd, xe_ptr buf, uint len, uint flags)
	XE_OP_DEFINE2(send, 			int fd, xe_cptr buf, uint len, uint flags)
	XE_OP_DEFINE2(recvmsg, 			int fd, msghdr* msg, uint flags)
	XE_OP_DEFINE2(sendmsg, 			int fd, const msghdr* msg, uint flags)
	XE_OP_DEFINE2(send_zc, 			int fd, xe_cptr buf, uint len, uint flags, uint buf_index = 0)
	XE_OP_DEFINE2(sendto_zc, 		int fd, xe_cptr buf, uint len, uint flags, const sockaddr* addr, socklen_t addrlen, uint buf_index = 0)

	XE_OP_DEFINE1(shutdown, 		int fd, int how)

	XE_OP_DEFINE2(poll, 			int fd, uint mask)
	XE_OP_DEFINE1(poll_update, 		xe_req& poll, uint mask, uint flags)
	XE_OP_DEFINE1(epoll_ctl, 		int epfd, int op, int fd, epoll_event* events)

	XE_OP_DEFINE1(poll_cancel, 		xe_req& cancel)
	XE_OP_DEFINE1(cancel, 			xe_req& cancel, uint flags)
	XE_OP_DEFINE1(cancel, 			int fd, uint flags)
	XE_OP_DEFINE1(cancel_fixed, 	uint file_index, uint flags)
	XE_OP_DEFINE1(cancel_all)

	XE_OP_DEFINE1(files_update, 	int* fds, uint len, uint offset)

	XE_OP_DEFINE1(provide_buffers, 	xe_ptr addr, uint len, ushort nr, ushort bgid, ushort bid)
	XE_OP_DEFINE1(remove_buffers, 	ushort nr, ushort bgid)

#undef XE_OP_DEFINE1
#undef XE_OP_DEFINE2

	~xe_loop() = default;

	static xe_cstr class_name();
};