#pragma once
#include <liburing.h>

#if !defined XE_COROUTINE_EXPERIMENTAL && defined __clang__ && __clang_major__ < 14
#define XE_COROUTINE_EXPERIMENTAL 1
#endif

#if XE_COROUTINE_EXPERIMENTAL == 1
#include <experimental/coroutine>

namespace std{
	using namespace std::experimental;
};

#else
#include <coroutine>
#endif

#include "xstd/types.h"
#include "xstd/rbtree.h"

enum xe_iobuf_size{
	XE_LOOP_IOBUF_SIZE = 16 * 1024,
	XE_LOOP_IOBUF_SIZE_LARGE = 256 * 1024
};

enum xe_loop_handle_type : byte{
	XE_LOOP_HANDLE_NONE = 0,
	XE_LOOP_HANDLE_DISCARD, /* any result on this handle type will be ignored */
	XE_LOOP_HANDLE_SOCKET,
	XE_LOOP_HANDLE_FILE,
	XE_LOOP_HANDLE_PROMISE,
	XE_LOOP_HANDLE_USER,

	XURL_CTX,

	XE_LOOP_HANDLE_LAST
};

struct xe_loop;
struct xe_loop_handle{
	typedef void (*xe_callback)(xe_loop& loop, xe_ptr data, ulong u1, ulong u2, int io_result);

	ulong u1;
	ulong u2;

	xe_ptr user_data;
	xe_callback callback; /* callback for user handles */
};

enum xe_timer_flags{
	XE_TIMER_NONE = 0x0,
	XE_TIMER_REPEAT = 0x1,
	XE_TIMER_ABS = 0x2,
	XE_TIMER_ALIGN = 0x4 /* set next timeout to expire time + repeat instead of now + repeat */
};

class xe_timer{
private:
	xe_rbtree<ulong>::node expire;
	ulong delay;

	bool active_: 1;
	bool repeat_: 1;
	bool align_: 1;
	bool in_callback: 1;

	friend class xe_loop;
public:
	typedef int (*xe_callback)(xe_loop& loop, xe_timer& timer);

	xe_callback callback;

	xe_timer();
	~xe_timer() = default;

	bool active() const;
	bool repeat() const;
	bool align() const;
};

struct xe_loop_options{
	uint capacity; /* number of simultaneous (unfinished) io requests */
	uint sq_thread_cpu;
	uint flag_sqpoll: 1;
	uint flag_iopoll: 1;
	uint flag_sqaff: 1;
	uint flag_iobuf: 1; /* loop allocates a buffer for sync I/O */
	uint flags: 28;

	xe_loop_options();
	~xe_loop_options() = default;
};

class xe_promise{
private:
	std::coroutine_handle<> waiter;

	int result_;
	int handle_;

	xe_promise();
	xe_promise(const xe_promise&) = delete;
	xe_promise& operator=(const xe_promise&) = delete;
	xe_promise(xe_promise&&) = default;
	xe_promise& operator=(xe_promise&&) = delete;

	friend class xe_loop;
public:
	bool await_ready();
	void await_suspend(std::coroutine_handle<> handle);
	int await_resume();

	int handle();

	~xe_promise(){}
};

class xe_loop{
private:
	io_uring ring;

	uint num_handles;
	uint num_queued;
	uint num_reserved;
	uint num_capacity; /* number of simultaneous (unfinished) io requests, equal to number of sqes in the ring */

	xe_ptr io_buf; /* shared buffer for sync I/O */

	xe_loop_handle* handles;

	xe_rbtree<ulong> timers;

	bool handle_invalid(int);
	int enter(uint, uint, uint, ulong);
	int submit(uint, ulong);
	int waitsingle(ulong);
	void run_timer(xe_timer&, ulong);
	void queue_timer(xe_timer&);

	template<typename F>
	int queue_io(int op, xe_ptr, xe_loop_handle::xe_callback, ulong, ulong, xe_loop_handle_type, F);
public:
	xe_loop();

	int init();
	int init_options(xe_loop_options& options);

	int run();

	uint remain();
	uint capacity();

	bool reserve(uint count);
	void release(uint count);

#define XE_LOOP_IO_ARGS xe_ptr user_data, xe_loop_handle::xe_callback callback, ulong u1 = 0, ulong u2 = 0, xe_loop_handle_type handle_type = XE_LOOP_HANDLE_USER
	int nop				(																			XE_LOOP_IO_ARGS);

	int openat			(int fd, xe_cstr path, uint flags, mode_t mode, 							XE_LOOP_IO_ARGS);
	int openat2			(int fd, xe_cstr path, struct open_how* how, 								XE_LOOP_IO_ARGS);

	int read			(int fd, xe_ptr buf, uint len, ulong offset, 								XE_LOOP_IO_ARGS);
	int write			(int fd, xe_cptr buf, uint len, ulong offset, 								XE_LOOP_IO_ARGS);
	int readv			(int fd, iovec* iovecs, uint vlen, ulong offset, 							XE_LOOP_IO_ARGS);
	int writev			(int fd, iovec* iovecs, uint vlen, ulong offset, 							XE_LOOP_IO_ARGS);
	int read_fixed		(int fd, xe_ptr buf, uint len, ulong offset, uint buf_index,				XE_LOOP_IO_ARGS);
	int write_fixed		(int fd, xe_cptr buf, uint len, ulong offset, uint buf_index,				XE_LOOP_IO_ARGS);

	int fsync			(int fd, uint flags,						 								XE_LOOP_IO_ARGS);
	int fallocate		(int fd, int mode, ulong offset, ulong len, 								XE_LOOP_IO_ARGS);
	int fadvise			(int fd, ulong offset, ulong len, uint advice, 								XE_LOOP_IO_ARGS);

	int madvise			(xe_ptr addr, ulong len, uint advice, 										XE_LOOP_IO_ARGS);

	int sync_file_range	(int fd, uint len, ulong offset, uint flags,								XE_LOOP_IO_ARGS);

	int statx			(int fd, xe_cstr path, uint flags, uint mask, struct statx* statx,			XE_LOOP_IO_ARGS);

	int renameat		(int old_fd, xe_cstr old_path, int new_fd, xe_cstr new_path, uint flags, 	XE_LOOP_IO_ARGS);
	int unlinkat		(int fd, xe_cstr path, uint flags, 											XE_LOOP_IO_ARGS);

	int splice			(int fd_in, ulong off_in, int fd_out, ulong off_out, uint len, uint flags, 	XE_LOOP_IO_ARGS);
	int tee				(int fd_in, int fd_out, uint len, uint flags, 								XE_LOOP_IO_ARGS);

	int connect			(int fd, sockaddr* addr, socklen_t addrlen, 								XE_LOOP_IO_ARGS);
	int accept			(int fd, sockaddr* addr, socklen_t* addrlen, uint flags, 					XE_LOOP_IO_ARGS);
	int recv			(int fd, xe_ptr buf, uint len, uint flags, 									XE_LOOP_IO_ARGS);
	int send			(int fd, xe_cptr buf, uint len, uint flags, 									XE_LOOP_IO_ARGS);

	int recvmsg			(int fd, struct msghdr* msg, uint flags, 									XE_LOOP_IO_ARGS);
	int sendmsg			(int fd, struct msghdr* msg, uint flags, 									XE_LOOP_IO_ARGS);

	int poll			(int fd, uint mask, 														XE_LOOP_IO_ARGS);
	int poll_cancel		(int handle, 																XE_LOOP_IO_ARGS);

	int cancel			(int handle, uint flags, 													XE_LOOP_IO_ARGS);

	int files_update	(int* fds, uint len, uint offset, 		 									XE_LOOP_IO_ARGS);

	int modify_handle	(int handle, xe_ptr user_data, xe_loop_handle::xe_callback callback, ulong u1 = 0, ulong u2 = 0);
#undef XE_LOOP_IO_ARGS
	xe_promise nop				();

	xe_promise openat			(int fd, xe_cstr path, uint flags, mode_t mode);
	xe_promise openat2			(int fd, xe_cstr path, struct open_how* how);

	xe_promise read				(int fd, xe_ptr buf, uint len, ulong offset);
	xe_promise write			(int fd, xe_cptr buf, uint len, ulong offset);
	xe_promise readv			(int fd, iovec* iovecs, uint vlen, ulong offset);
	xe_promise writev			(int fd, iovec* iovecs, uint vlen, ulong offset);
	xe_promise read_fixed		(int fd, xe_ptr buf, uint len, ulong offset, uint buf_index);
	xe_promise write_fixed		(int fd, xe_cptr buf, uint len, ulong offset, uint buf_index);

	xe_promise fsync			(int fd, uint flags);
	xe_promise fallocate		(int fd, int mode, ulong offset, ulong len);
	xe_promise fadvise			(int fd, ulong offset, ulong len, uint advice);

	xe_promise madvise			(xe_ptr addr, ulong len, uint advice);

	xe_promise sync_file_range	(int fd, uint len, ulong offset, uint flags);

	xe_promise statx			(int fd, xe_cstr path, uint flags, uint mask, struct statx* statx);

	xe_promise renameat			(int old_fd, xe_cstr old_path, int new_fd, xe_cstr new_path, uint flags);
	xe_promise unlinkat			(int fd, xe_cstr path, uint flags);

	xe_promise splice			(int fd_in, ulong off_in, int fd_out, ulong off_out, uint len, uint flags);
	xe_promise tee				(int fd_in, int fd_out, uint len, uint flags);

	xe_promise connect			(int fd, sockaddr* addr, socklen_t addrlen);
	xe_promise accept			(int fd, sockaddr* addr, socklen_t* addrlen, uint flags);
	xe_promise recv				(int fd, xe_ptr buf, uint len, uint flags);
	xe_promise send				(int fd, xe_cptr buf, uint len, uint flags);

	xe_promise recvmsg			(int fd, struct msghdr* msg, uint flags);
	xe_promise sendmsg			(int fd, struct msghdr* msg, uint flags);

	xe_promise poll				(int fd, uint mask);
	xe_promise poll_cancel		(int handle);

	xe_promise cancel			(int handle, uint flags);

	xe_promise files_update		(int* fds, uint len, uint offset);

	int timer_ms(xe_timer& timer, ulong time, ulong repeat, uint flags);
	int timer_ns(xe_timer& timer, ulong time, ulong repeat, uint flags);
	int timer_cancel(xe_timer& timer);

	xe_ptr iobuf() const;
	xe_ptr iobuf_large() const;

	void close();

	~xe_loop() = default;

	static xe_cstr class_name();
};