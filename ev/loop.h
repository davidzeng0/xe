#pragma once
#include <liburing.h>
#include "xe/types.h"

#define XE_LOOP_IOBUF_SIZE (16 * 1024)
#define XE_LOOP_IOBUF_LARGE_SIZE (256 * 1024)

struct xe_loop;

enum xe_handle_type : byte{
	XE_LOOP_NONE = 0,
	XE_LOOP_DISCARD, /* any result on this handle type will be ignored */
	XE_LOOP_TIMER,
	XE_LOOP_SOCKET,
	XE_LOOP_FILE,
	XE_LOOP_IOHANDLE,
	XE_LOOP_PROMISE,

	XE_NET,

	XE_LOOP_LAST
};

struct xe_handle{
	typedef void (*xe_callback)(xe_loop* loop, xe_ptr data, ulong u1, ulong u2, int io_result);

	ulong u1;
	ulong u2;

	xe_ptr user_data;
	xe_callback callback; /* callback for {XE_LOOP_IOHANDLE} */
};

struct xe_timer{
	typedef __kernel_timespec xe_timespec;
	typedef void (*xe_callback)(xe_loop* loop, xe_timer* timer);

	xe_timespec expire;
	ulong start;
	ulong delay;
	uint flags;
	int cancel;

	xe_callback callback;
};

struct xe_loop_options{
	enum xe_flags{
		XE_FLAGS_NONE   = 0x0,
		XE_FLAGS_SQPOLL = 0x1,
		XE_FLAGS_IOPOLL = 0x2,
		XE_FLAGS_SQAFF  = 0x4,
		XE_FLAGS_IOBUF  = 0x8 /* loop allocates a buffer for sync I/O */
	};

	uint capacity; /* number of simultaneous (unfinished) io requests */
	uint flags;
	uint sq_thread_cpu;
	uint pad;
};

struct xe_loop{
	io_uring ring;
	xe_handle* handles;

	uint num_handles;
	uint num_queued;
	uint num_reserved;

	uint capacity; /* number of simultaneous (unfinished) io requests, is equal to number of sqes in {ring} */

	xe_buf io_buf; /* shared buffer for sync I/O */
};

int xe_loop_init(xe_loop* loop);
int xe_loop_init_options(xe_loop* loop, xe_loop_options* options);

int xe_loop_run(xe_loop* loop);

uint xe_loop_remain(xe_loop* loop);

bool xe_loop_reserve(xe_loop* loop, uint count);

void xe_loop_unreserve(xe_loop* loop, uint count);

#define XE_IO_ARGS xe_handle_type handle_type, xe_ptr user_data, xe_handle::xe_callback callback, ulong u1, ulong u2

int xe_loop_nop				(xe_loop* loop, 																			XE_IO_ARGS);

int xe_loop_openat			(xe_loop* loop, int fd, xe_cstr path, uint flags, mode_t mode, 								XE_IO_ARGS);
int xe_loop_openat2			(xe_loop* loop, int fd, xe_cstr path, struct open_how* how, 								XE_IO_ARGS);

int xe_loop_read			(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset, 								XE_IO_ARGS);
int xe_loop_write			(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset, 								XE_IO_ARGS);
int xe_loop_readv			(xe_loop* loop, int fd, iovec* iovecs, uint vlen, ulong offset, 							XE_IO_ARGS);
int xe_loop_writev			(xe_loop* loop, int fd, iovec* iovecs, uint vlen, ulong offset, 							XE_IO_ARGS);
int xe_loop_read_fixed		(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset, uint buf_index,					XE_IO_ARGS);
int xe_loop_write_fixed		(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset, uint buf_index,					XE_IO_ARGS);

int xe_loop_fsync			(xe_loop* loop, int fd, uint flags,						 									XE_IO_ARGS);
int xe_loop_fallocate		(xe_loop* loop, int fd, int mode, ulong offset, ulong len, 									XE_IO_ARGS);
int xe_loop_fadvise			(xe_loop* loop, int fd, ulong offset, ulong len, uint advice, 								XE_IO_ARGS);

int xe_loop_madvise			(xe_loop* loop, xe_ptr addr, ulong len, uint advice, 										XE_IO_ARGS);

int xe_loop_sync_file_range	(xe_loop* loop, int fd, uint len, ulong offset, uint flags,									XE_IO_ARGS);

int xe_loop_statx			(xe_loop* loop, int fd, xe_cstr path, uint flags, uint mask, struct statx* statx,			XE_IO_ARGS);

int xe_loop_renameat		(xe_loop* loop, int old_fd, xe_cstr old_path, int new_fd, xe_cstr new_path, uint flags, 	XE_IO_ARGS);
int xe_loop_unlinkat		(xe_loop* loop, int fd, xe_cstr path, uint flags, 											XE_IO_ARGS);

int xe_loop_splice			(xe_loop* loop, int fd_in, ulong off_in, int fd_out, ulong off_out, uint len, uint flags, 	XE_IO_ARGS);
int xe_loop_tee				(xe_loop* loop, int fd_in, int fd_out, uint len, uint flags, 								XE_IO_ARGS);

int xe_loop_connect			(xe_loop* loop, int fd, sockaddr* addr, socklen_t addrlen, 									XE_IO_ARGS);
int xe_loop_accept			(xe_loop* loop, int fd, sockaddr* addr, socklen_t* addrlen, uint flags, 					XE_IO_ARGS);
int xe_loop_recv			(xe_loop* loop, int fd, xe_buf buf, uint len, uint flags, 									XE_IO_ARGS);
int xe_loop_send			(xe_loop* loop, int fd, xe_buf buf, uint len, uint flags, 									XE_IO_ARGS);

int xe_loop_recvmsg			(xe_loop* loop, int fd, struct msghdr* msg, uint flags, 									XE_IO_ARGS);
int xe_loop_sendmsg			(xe_loop* loop, int fd, struct msghdr* msg, uint flags, 									XE_IO_ARGS);

int xe_loop_poll			(xe_loop* loop, int fd, uint mask, 															XE_IO_ARGS);
int xe_loop_poll_cancel		(xe_loop* loop, int handle, 																XE_IO_ARGS);

int xe_loop_cancel			(xe_loop* loop, int handle, uint flags, 													XE_IO_ARGS);

int xe_loop_files_update	(xe_loop* loop, int* fds, uint len, uint offset, 		 									XE_IO_ARGS);

int xe_loop_modify_handle	(xe_loop* loop, int handle, xe_ptr user_data, xe_handle::xe_callback callback, ulong u1, ulong u2);

#undef XE_IO_ARGS

int xe_loop_timer_ms(xe_loop* loop, xe_timer* timer, ulong time, bool repeat);
int xe_loop_timer_ns(xe_loop* loop, xe_timer* timer, ulong time, bool repeat);
int xe_loop_timer_cancel(xe_loop* loop, xe_timer* timer);

xe_buf xe_loop_iobuf(xe_loop* loop);
xe_buf xe_loop_iobuf_large(xe_loop* loop);

void xe_loop_destroy(xe_loop* loop);

ulong xe_time_ns(); /* system time in nanoseconds */
ulong xe_time_ms(); /* system time in milliseconds */