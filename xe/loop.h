#pragma once
#include <liburing.h>

#if !defined XE_COROUTINE_EXPERIMENTAL && defined __clang__
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

#include "xutil/types.h"

enum{
	XE_LOOP_IOBUF_SIZE = 16 * 1024,
	XE_LOOP_IOBUF_SIZE_LARGE = 256 * 1024
};

struct xe_loop;

enum xe_loop_handle_type : byte{
	XE_LOOP_HANDLE_NONE = 0,
	XE_LOOP_HANDLE_DISCARD, /* any result on this handle type will be ignored */
	XE_LOOP_HANDLE_TIMER,
	XE_LOOP_HANDLE_SOCKET,
	XE_LOOP_HANDLE_FILE,
	XE_LOOP_HANDLE_PROMISE,
	XE_LOOP_HANDLE_USER,

	XURL_CONNECTION,
	XURL_RESOLVER,

	XE_LOOP_HANDLE_LAST
};

struct xe_loop_handle{
	typedef void (*xe_callback)(xe_loop& loop, xe_ptr data, ulong u1, ulong u2, int io_result);

	ulong u1;
	ulong u2;

	xe_ptr user_data;
	xe_callback callback; /* callback for {XE_LOOP_HANDLE_USER} */
};

struct xe_timer{
	typedef __kernel_timespec xe_timespec;
	typedef void (*xe_callback)(xe_loop& loop, xe_timer& timer);

	xe_timespec expire;
	xe_callback callback;
	ulong start;
	ulong delay;
	uint flags;
	int cancel;

	xe_timer();
};

enum xe_loop_flags{
	XE_LOOP_FLAGS_NONE   = 0x0,
	XE_LOOP_FLAGS_SQPOLL = 0x1,
	XE_LOOP_FLAGS_IOPOLL = 0x2,
	XE_LOOP_FLAGS_SQAFF  = 0x4,
	XE_LOOP_FLAGS_IOBUF  = 0x8 /* loop allocates a buffer for sync I/O */
};

struct xe_loop_options{
	uint capacity; /* number of simultaneous (unfinished) io requests */
	uint flags;
	uint sq_thread_cpu;
	uint pad;

	xe_loop_options();
};

class xe_promise{
private:
	std::coroutine_handle<> waiter;

	int result_;
	int handle_;

	xe_promise();

	friend class xe_loop;
public:
	bool await_ready();
	void await_suspend(std::coroutine_handle<> handle);
	int await_resume();

	int handle();
};

class xe_loop{
private:
	io_uring ring;

	uint num_handles;
	uint num_queued;
	uint num_reserved;
	uint num_capacity; /* number of simultaneous (unfinished) io requests, is equal to number of sqes in {ring} */

	xe_buf io_buf; /* shared buffer for sync I/O */

	xe_loop_handle* handles;

	bool handle_invalid(int);
	int enter(uint, uint, uint);
	int submit(uint);
	int waitsingle();
	void run_timer(xe_timer&);
	int queue_timer(xe_timer&);
	int queue_timer_internal(xe_timer&);

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

	int read			(int fd, xe_buf buf, uint len, ulong offset, 								XE_LOOP_IO_ARGS);
	int write			(int fd, xe_buf buf, uint len, ulong offset, 								XE_LOOP_IO_ARGS);
	int readv			(int fd, iovec* iovecs, uint vlen, ulong offset, 							XE_LOOP_IO_ARGS);
	int writev			(int fd, iovec* iovecs, uint vlen, ulong offset, 							XE_LOOP_IO_ARGS);
	int read_fixed		(int fd, xe_buf buf, uint len, ulong offset, uint buf_index,				XE_LOOP_IO_ARGS);
	int write_fixed		(int fd, xe_buf buf, uint len, ulong offset, uint buf_index,				XE_LOOP_IO_ARGS);

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
	int recv			(int fd, xe_buf buf, uint len, uint flags, 									XE_LOOP_IO_ARGS);
	int send			(int fd, xe_buf buf, uint len, uint flags, 									XE_LOOP_IO_ARGS);

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

	xe_promise read				(int fd, xe_buf buf, uint len, ulong offset);
	xe_promise write			(int fd, xe_buf buf, uint len, ulong offset);
	xe_promise readv			(int fd, iovec* iovecs, uint vlen, ulong offset);
	xe_promise writev			(int fd, iovec* iovecs, uint vlen, ulong offset);
	xe_promise read_fixed		(int fd, xe_buf buf, uint len, ulong offset, uint buf_index);
	xe_promise write_fixed		(int fd, xe_buf buf, uint len, ulong offset, uint buf_index);

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
	xe_promise recv				(int fd, xe_buf buf, uint len, uint flags);
	xe_promise send				(int fd, xe_buf buf, uint len, uint flags);

	xe_promise recvmsg			(int fd, struct msghdr* msg, uint flags);
	xe_promise sendmsg			(int fd, struct msghdr* msg, uint flags);

	xe_promise poll				(int fd, uint mask);
	xe_promise poll_cancel		(int handle);

	xe_promise cancel			(int handle, uint flags);

	xe_promise files_update		(int* fds, uint len, uint offset);

	int timer_ms(xe_timer& timer, ulong time, bool repeat);
	int timer_ns(xe_timer& timer, ulong time, bool repeat);
	int timer_cancel(xe_timer& timer);

	xe_buf iobuf() const;
	xe_buf iobuf_large() const;

	void destroy();

	static xe_cstr class_name();
};