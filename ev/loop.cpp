#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>
#include "loop.h"
#include "xe/debug.h"
#include "xe/common.h"
#include "xe/mem.h"
#include "xe/error.h"
#include "net/net.h"
#include "socket.h"
#include "file.h"
#include "arch.h"
#include "async.h"

#define ENTRY_COUNT 1024  /* default sqe and cqe count */
#define MAX_ENTRIES 16777216 /* max sqe count permitted to allow packing xe_handle_type and handle index into a signed int */

enum xe_timer_flags{
	XE_TIMER_NONE       = 0x0,
	XE_TIMER_ACTIVE     = 0x1,
	XE_TIMER_REPEAT     = 0x2,
	XE_TIMER_CALLBACK   = 0x4,
	XE_TIMER_CANCELLING = 0x8
};

int xe_loop_init(xe_loop* loop){
	xe_loop_options options;

	xe_zero(&options);

	return xe_loop_init_options(loop, &options);
}

int xe_loop_init_options(xe_loop* loop, xe_loop_options* options){
	xe_buf io_buf;
	xe_handle* handles;
	io_uring_params params;

	if(!options)
		return XE_EINVAL;
	if(options -> flags & ~(xe_loop_options::XE_FLAGS_SQPOLL | xe_loop_options::XE_FLAGS_IOPOLL | xe_loop_options::XE_FLAGS_SQAFF | xe_loop_options::XE_FLAGS_IOBUF))
		return XE_EINVAL;
	if(!options -> capacity)
		options -> capacity = ENTRY_COUNT;
	else if(options -> capacity > MAX_ENTRIES)
		return XE_ETOOMANYHANDLES;
	io_buf = null;
	handles = null;

	if(options -> flags & xe_loop_options::XE_FLAGS_IOBUF){
		io_buf = xe_aligned_alloc<byte>(XE_PAGESIZE, XE_LOOP_IOBUF_LARGE_SIZE);

		if(!io_buf)
			return XE_ENOMEM;
		/* force memory allocation */
		xe_zero((byte*)io_buf, XE_LOOP_IOBUF_LARGE_SIZE);
	}

	xe_zero(&params);

	params.cq_entries = options -> capacity;
	params.flags = IORING_SETUP_CQSIZE;

	if(options -> flags & xe_loop_options::XE_FLAGS_IOPOLL)
		params.flags |= IORING_SETUP_IOPOLL;
	if(options -> flags & xe_loop_options::XE_FLAGS_SQPOLL)
		params.flags |= IORING_SETUP_SQPOLL;
	if(options -> flags & xe_loop_options::XE_FLAGS_SQAFF){
		params.flags |= IORING_SETUP_SQ_AFF;
		params.sq_thread_cpu = options -> sq_thread_cpu;
	}

	int err = io_uring_queue_init_params(options -> capacity, &loop -> ring, &params);

	if(err){
		if(io_buf)
			xe_dealloc(io_buf);
		return err;
	}

	handles = xe_aligned_alloc<xe_handle>(XE_PAGESIZE, params.sq_entries);

	if(!handles){
		io_uring_queue_exit(&loop -> ring);

		if(io_buf)
			xe_dealloc(io_buf);
		return XE_ENOMEM;
	}

	/* force memory allocation */
	xe_zero(handles, params.sq_entries);

	for(uint i = 0; i < params.sq_entries; i++){
		loop -> ring.sq.sqes[i].user_data = i; /* link each sqe to an {xe_handle} */
		loop -> ring.sq.array[i] = i; /* pre-fill the sqe array */
	}

	loop -> handles = handles;
	loop -> capacity = params.sq_entries;
	loop -> ring.sq.sqe_head = *loop -> ring.sq.kring_mask; /* sqe_head will be used as the sqe mask */
	loop -> io_buf = io_buf;

	return 0;
}

void xe_loop_destroy(xe_loop* loop){
	io_uring_queue_exit(&loop -> ring);
	xe_dealloc(loop -> handles);

	if(loop -> io_buf)
		xe_dealloc(loop -> io_buf);
}

static inline int xe_handle_packed_type(xe_handle_type handle_type){
	return ((uint)handle_type << 24);
}

static inline xe_handle_type xe_handle_unpack_type(ulong packed){
	return (xe_handle_type)(packed >> 24);
}

static inline uint xe_handle_unpack_index(ulong packed){
	return packed & ((1 << 24) - 1);
}

static inline bool xe_handle_invalid(xe_loop* loop, int handle){
	return handle < 0 ||
		xe_handle_unpack_index(handle) >= loop -> capacity ||
		xe_handle_unpack_type(handle) <= XE_LOOP_NONE ||
		xe_handle_unpack_type(handle) >= XE_LOOP_LAST;
}

#define XE_IO_ARGS xe_handle_type handle_type, xe_ptr user_data, xe_handle::xe_callback callback, ulong u1, ulong u2

#define xe_queue_sqe_head(op)																		\
	xe_assert(loop -> num_queued <= loop -> capacity);												\
	xe_assert(loop -> num_handles <= loop -> capacity);												\
	xe_assert(loop -> num_reserved <= loop -> capacity);											\
	xe_assert(loop -> num_queued + loop -> num_handles + loop -> num_reserved <= loop -> capacity);	\
																									\
	io_uring_sqe* sqe;																				\
	xe_handle* handle;																				\
																									\
	if((handle_type <= XE_LOOP_NONE || handle_type >= XE_LOOP_LAST) ||								\
		(handle_type == XE_LOOP_IOHANDLE && callback == null))										\
		return XE_EINVAL;																			\
	if(loop -> num_queued + loop -> num_handles + loop -> num_reserved >= loop -> capacity)			\
		return XE_ETOOMANYHANDLES;																	\
	loop -> num_queued++;																			\
	sqe = &loop -> ring.sq.sqes[loop -> ring.sq.sqe_tail++ & loop -> ring.sq.sqe_head];				\
																									\
	xe_assert(sqe -> user_data < loop -> capacity); /* {sqe -> user_data} is the index of the {xe_handle} that is linked to this sqe */	\
																									\
	/* copy io parameters */																		\
	sqe -> opcode = op;

#define xe_queue_sqe_tail									\
	handle = &loop -> handles[sqe -> user_data];			\
															\
	/* copy io handle information */						\
	handle -> user_data = user_data;						\
	handle -> u1 = u1;										\
	handle -> u2 = u2;										\
															\
	if(handle_type == XE_LOOP_IOHANDLE)						\
		handle -> callback = callback;						\
	sqe -> user_data |= xe_handle_packed_type(handle_type);	\
															\
	return sqe -> user_data;

static inline void xe_sqe_rw(io_uring_sqe* sqe, uint flags, int fd, const void* addr, uint len, ulong offset, uint rw_flags, uint buf_index){
	sqe -> flags = flags;
	sqe -> fd = fd;
	sqe -> addr = (ulong)addr;
	sqe -> len = len;
	sqe -> off = offset;
	sqe -> rw_flags = rw_flags;
	sqe -> buf_index = buf_index;
}

static inline void xe_sqe_socket_rw(io_uring_sqe* sqe, uint flags, int fd, const void* addr, uint len, uint rw_flags){
	sqe -> flags = flags;
	sqe -> fd = fd;
	sqe -> addr = (ulong)addr;
	sqe -> len = len;
	sqe -> rw_flags = rw_flags;
}

static inline void xe_sqe_pipe(io_uring_sqe* sqe, uint flags, int fd_in, ulong off_in, int fd_out, ulong off_out, uint len, uint rw_flags){
	sqe -> flags = flags;
	sqe -> fd = fd_out;
	sqe -> len = len;
	sqe -> off = off_out;
	sqe -> splice_off_in = off_in;
	sqe -> splice_fd_in = fd_in;
	sqe -> splice_flags = rw_flags;
}

static inline int xe_sqe_timer(xe_loop* loop, xe_timer* timer, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_TIMEOUT)

	sqe -> flags = 0;
	sqe -> addr = (ulong)&timer -> expire;
	sqe -> len = 1;
	sqe -> off = 0;
	sqe -> rw_flags = IORING_TIMEOUT_ABS;
	sqe -> buf_index = 0;

	xe_queue_sqe_tail
}

static inline int xe_sqe_timer_cancel(xe_loop* loop, xe_timer* timer, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_TIMEOUT_REMOVE)

	sqe -> flags = 0;
	sqe -> addr = (ulong)timer -> cancel;
	sqe -> len = 0;
	sqe -> rw_flags = 0;
	sqe -> buf_index = 0;

	xe_queue_sqe_tail
}

static int xe_queue_timer_internal(xe_loop* loop, xe_timer* timer){
	int res = xe_sqe_timer(loop, timer, XE_LOOP_TIMER, timer, null, 0, 0);

	if(res >= 0)
		timer -> cancel = res;
	return res;
}

static int xe_queue_timer(xe_loop* loop, xe_timer* timer){
	if(timer -> flags & XE_TIMER_ACTIVE)
		return XE_ETIMERALREADYACTIVE;
	if(timer -> flags & XE_TIMER_CALLBACK){
		/* an sqe was reserved before callback, just mark as active */
		timer -> flags |= XE_TIMER_ACTIVE;

		return 0;
	}

	int ret = xe_queue_timer_internal(loop, timer);

	if(ret >= 0){
		timer -> flags |= XE_TIMER_ACTIVE;

		return 0;
	}

	return ret;
}

int xe_loop_timer_ms(xe_loop* loop, xe_timer* timer, ulong millis, bool repeat){
	return xe_loop_timer_ns(loop, timer, millis * (ulong)1e6, repeat);
}

int xe_loop_timer_ns(xe_loop* loop, xe_timer* timer, ulong nanos, bool repeat){
	timer -> start = xe_time_ns();
	timer -> delay = nanos;
	timer -> expire.tv_nsec = timer -> start + nanos;

	if(repeat)
		timer -> flags |= XE_TIMER_REPEAT;
	else
		timer -> flags &= ~XE_TIMER_REPEAT;
	return xe_queue_timer(loop, timer);
}

int xe_loop_timer_cancel(xe_loop* loop, xe_timer* timer){
	if(!(timer -> flags & XE_TIMER_ACTIVE))
		return XE_ETIMERNOTACTIVE;
	if(timer -> flags & XE_TIMER_CANCELLING)
		return 0;
	xe_assert(!xe_handle_invalid(loop, timer -> cancel));

	int ret = xe_sqe_timer_cancel(loop, timer, XE_LOOP_DISCARD, null, null, 0, 0);

	if(ret >= 0){
		timer -> flags &= ~XE_TIMER_REPEAT;
		timer -> flags |= XE_TIMER_CANCELLING;

		return 0;
	}

	return ret;
}

int xe_loop_nop(xe_loop* loop, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_NOP)

	sqe -> flags = 0;

	xe_queue_sqe_tail
}

int xe_loop_openat(xe_loop* loop, int fd, xe_cstr path, uint flags, mode_t mode, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_OPENAT)

	sqe -> flags = 0;
	sqe -> fd = fd;
	sqe -> addr = (ulong)path;
	sqe -> len = mode;
	sqe -> rw_flags = flags;
	sqe -> buf_index = 0;

	xe_queue_sqe_tail
}

int xe_loop_openat2(xe_loop* loop, int fd, xe_cstr path, struct open_how* how, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_OPENAT2)

	sqe -> flags = 0;
	sqe -> fd = fd;
	sqe -> addr = (ulong)path;
	sqe -> len = sizeof(*how);
	sqe -> off = (ulong)how;
	sqe -> buf_index = 0;

	xe_queue_sqe_tail
}

int xe_loop_read(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_READ)
	xe_sqe_rw(sqe, IOSQE_ASYNC, fd, buf, len, offset, 0, 0);
	xe_queue_sqe_tail
}

int xe_loop_write(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_WRITE)
	xe_sqe_rw(sqe, IOSQE_ASYNC, fd, buf, len, offset, 0, 0);
	xe_queue_sqe_tail
}

int xe_loop_readv(xe_loop* loop, int fd, iovec* vecs, uint vlen, ulong offset, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_READV)
	xe_sqe_rw(sqe, IOSQE_ASYNC, fd, vecs, vlen, offset, 0, 0);
	xe_queue_sqe_tail
}

int xe_loop_writev(xe_loop* loop, int fd, iovec* vecs, uint vlen, ulong offset, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_WRITEV)
	xe_sqe_rw(sqe, IOSQE_ASYNC, fd, vecs, vlen, offset, 0, 0);
	xe_queue_sqe_tail
}

int xe_loop_read_fixed(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset, uint buf_index, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_READ_FIXED)
	xe_sqe_rw(sqe, IOSQE_ASYNC, fd, buf, len, offset, 0, buf_index);
	xe_queue_sqe_tail
}

int xe_loop_write_fixed(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset, uint buf_index, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_WRITE_FIXED)
	xe_sqe_rw(sqe, IOSQE_ASYNC, fd, buf, len, offset, 0, buf_index);
	xe_queue_sqe_tail
}

int xe_loop_fsync(xe_loop* loop, int fd, uint flags, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_FSYNC)

	sqe -> flags = 0;
	sqe -> fd = fd;
	sqe -> addr = 0;
	sqe -> rw_flags = flags;
	sqe -> buf_index = 0;

	xe_queue_sqe_tail
}

int xe_loop_files_update(xe_loop* loop, int* fds, uint len, uint offset, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_FILES_UPDATE)

	sqe -> flags = 0;
	sqe -> addr = (ulong)fds;
	sqe -> len = len;
	sqe -> off = offset;
	sqe -> rw_flags = 0;

	xe_queue_sqe_tail
}

int xe_loop_fallocate(xe_loop* loop, int fd, int mode, ulong offset, ulong len, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_FALLOCATE)
	xe_sqe_rw(sqe, 0, fd, (xe_ptr)len, mode, offset, 0, 0);
	xe_queue_sqe_tail
}

int xe_loop_fadvise(xe_loop* loop, int fd, ulong offset, ulong len, uint advice, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_FADVISE)
	xe_sqe_rw(sqe, 0, fd, null, len, offset, advice, 0);
	xe_queue_sqe_tail
}

int xe_loop_sync_file_range(xe_loop* loop, int fd, uint len, ulong offset, uint flags, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_SYNC_FILE_RANGE)
	xe_sqe_rw(sqe, 0, fd, null, len, offset, flags, 0);
	xe_queue_sqe_tail
}

int xe_loop_statx(xe_loop* loop, int fd, xe_cstr path, uint flags, uint mask, struct statx* statx, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_STATX)
	xe_sqe_rw(sqe, 0, fd, path, mask, (ulong)statx, flags, 0);
	xe_queue_sqe_tail
}

int xe_loop_renameat(xe_loop* loop, int old_fd, xe_cstr old_path, int new_fd, xe_cstr new_path, uint flags, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_RENAMEAT)

	sqe -> flags = 0;
	sqe -> fd = old_fd;
	sqe -> addr = (ulong)old_path;
	sqe -> len = new_fd;
	sqe -> off = (ulong)new_path;
	sqe -> rw_flags = flags;

	xe_queue_sqe_tail
}

int xe_loop_unlinkat(xe_loop* loop, int fd, xe_cstr path, uint flags, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_UNLINKAT)

	sqe -> flags = 0;
	sqe -> fd = fd;
	sqe -> addr = (ulong)path;
	sqe -> rw_flags = flags;

	xe_queue_sqe_tail
}

int xe_loop_splice(xe_loop* loop, int fd_in, ulong off_in, int fd_out, ulong off_out, uint len, uint flags, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_SPLICE)
	xe_sqe_pipe(sqe, 0, fd_in, off_in, fd_out, off_out, len, flags);
	xe_queue_sqe_tail
}

int xe_loop_tee(xe_loop* loop, int fd_in, int fd_out, uint len, uint flags, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_TEE)
	xe_sqe_pipe(sqe, 0, fd_in, 0, fd_out, 0, len, flags);
	xe_queue_sqe_tail
}

int xe_loop_connect(xe_loop* loop, int fd, sockaddr* addr, socklen_t addrlen, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_CONNECT)
	xe_sqe_rw(sqe, IOSQE_ASYNC, fd, addr, 0, addrlen, 0, 0);
	xe_queue_sqe_tail
}

int xe_loop_accept(xe_loop* loop, int fd, sockaddr* addr, socklen_t* addrlen, uint flags, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_ACCEPT)
	xe_sqe_rw(sqe, 0, fd, addr, 0, (ulong)addrlen, flags, 0);
	xe_queue_sqe_tail
}

int xe_loop_recv(xe_loop* loop, int fd, xe_buf buf, uint len, uint flags, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_RECV)
	xe_sqe_socket_rw(sqe, IOSQE_ASYNC, fd, buf, len, flags);
	xe_queue_sqe_tail
}

int xe_loop_send(xe_loop* loop, int fd, xe_buf buf, uint len, uint flags, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_SEND)
	xe_sqe_socket_rw(sqe, IOSQE_ASYNC, fd, buf, len, flags);
	xe_queue_sqe_tail
}

int xe_loop_recvmsg(xe_loop* loop, int fd, struct msghdr* msg, uint flags, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_RECVMSG)
	xe_sqe_socket_rw(sqe, IOSQE_ASYNC, fd, msg, 1, flags);
	xe_queue_sqe_tail
}

int xe_loop_sendmsg(xe_loop* loop, int fd, struct msghdr* msg, uint flags, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_SENDMSG)
	xe_sqe_socket_rw(sqe, IOSQE_ASYNC, fd, msg, 1, flags);
	xe_queue_sqe_tail
}

int xe_loop_poll(xe_loop* loop, int fd, uint mask, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_POLL_ADD)
	xe_sqe_rw(sqe, 0, fd, null, 0, 0, mask, 0);
	xe_queue_sqe_tail
}

int xe_loop_poll_cancel(xe_loop* loop, int hd, XE_IO_ARGS){
	if(xe_handle_invalid(loop, hd))
		return XE_EINVAL;
	xe_queue_sqe_head(IORING_OP_POLL_REMOVE)

	sqe -> flags = 0;
	sqe -> addr = (ulong)hd;
	sqe -> len = 0;
	sqe -> off = 0;
	sqe -> rw_flags = 0;
	sqe -> buf_index = 0;

	xe_queue_sqe_tail
}

int xe_loop_madvise(xe_loop* loop, xe_ptr addr, ulong len, uint advice, XE_IO_ARGS){
	xe_queue_sqe_head(IORING_OP_MADVISE)

	sqe -> flags = 0;
	sqe -> addr = (ulong)addr;
	sqe -> len = len;
	sqe -> off = 0;
	sqe -> rw_flags = advice;
	sqe -> buf_index = 0;

	xe_queue_sqe_tail
}

int xe_loop_cancel(xe_loop* loop, int hd, uint flags, XE_IO_ARGS){
	if(xe_handle_invalid(loop, hd))
		return XE_EINVAL;
	xe_queue_sqe_head(IORING_OP_ASYNC_CANCEL)

	sqe -> flags = 0;
	sqe -> addr = (ulong)hd;
	sqe -> len = 0;
	sqe -> off = 0;
	sqe -> rw_flags = flags;

	xe_queue_sqe_tail
}

int xe_loop_modify_handle(xe_loop* loop, int hd, xe_ptr user_data, xe_handle::xe_callback callback, ulong u1, ulong u2){
	if(xe_handle_invalid(loop, hd))
		return XE_EINVAL;
	xe_handle* handle = &loop -> handles[xe_handle_unpack_index(hd)];

	handle -> u1 = u1;
	handle -> u2 = u2;
	handle -> user_data = user_data;
	handle -> callback = callback;

	return 0;
}

ulong xe_time_ns(){
	timespec spec;

	clock_gettime(CLOCK_MONOTONIC, &spec);

	return spec.tv_nsec + spec.tv_sec * (ulong)1e9;
}

ulong xe_time_ms(){
	timespec spec;

	clock_gettime(CLOCK_MONOTONIC, &spec);

	return spec.tv_nsec / ((ulong)1e6) + spec.tv_sec * (ulong)1e3;
}

static int xe_loop_submit(xe_loop* loop, uint wait){
	uint flags = 0;
	uint submit = loop -> ring.sq.sqe_tail - *loop -> ring.sq.khead;

	int ret;

	io_uring_smp_store_release(loop -> ring.sq.ktail, loop -> ring.sq.sqe_tail);

	if(!(loop -> ring.flags & IORING_SETUP_IOPOLL))
		goto enter;
	if(uring_unlikely(IO_URING_READ_ONCE(*loop -> ring.sq.kflags) & IORING_SQ_NEED_WAKEUP)){
		flags |= IORING_ENTER_SQ_WAKEUP;

		goto enter;
	}

	if(wait)
		goto enter;
	return submit;

	enter:

	if(wait || (loop -> ring.flags & IORING_SETUP_IOPOLL))
		flags |= IORING_ENTER_GETEVENTS;
	ret = syscall(__NR_io_uring_enter, loop -> ring.ring_fd, submit, wait, flags, nullptr, _NSIG / 8);

	if(ret < 0)
		return -errno;
	return ret;
}

uint xe_loop_remain(xe_loop* loop){
	return loop -> capacity - loop -> num_handles - loop -> num_queued - loop -> num_reserved;
}

bool xe_loop_reserve(xe_loop* loop, uint count){
	if(xe_loop_remain(loop) >= count){
		loop -> num_reserved += count;

		return true;
	}

	return false;
}

void xe_loop_unreserve(xe_loop* loop, uint count){
	if(loop -> num_reserved >= count)
		loop -> num_reserved -= count;
}

static void xe_loop_timer(xe_loop* loop, xe_handle* handle, xe_timer* timer){
	ulong time_now, time_align;

	loop -> num_queued++; /* reserve an sqe for the timer */
	timer -> flags &= ~(XE_TIMER_ACTIVE | XE_TIMER_CANCELLING);
	timer -> flags |= XE_TIMER_CALLBACK;
	timer -> callback(loop, timer);
	timer -> flags &= ~XE_TIMER_CALLBACK;
	loop -> num_queued--;

	if(timer -> flags & XE_TIMER_ACTIVE)
		/* timer was set in callback */
		xe_queue_timer_internal(loop, timer);
	else if(timer -> flags & XE_TIMER_REPEAT){
		time_now = xe_time_ns();

		if(timer -> delay > 0){
			time_align = (time_now - timer -> start - 1) % timer -> delay;
			timer -> expire.tv_nsec = time_now + timer -> delay - time_align - 1;
			timer -> start = timer -> expire.tv_nsec - timer -> delay;
		}else{
			timer -> start = time_now;
			timer -> expire.tv_nsec = timer -> start;
		}

		xe_queue_timer(loop, timer);

		timer -> flags |= XE_TIMER_ACTIVE;
	}
}

static void xe_loop_process_handle(xe_loop* loop, xe_handle_type handle_type, xe_handle* handle, int res){
	switch(handle_type){
		case XE_LOOP_DISCARD:
			break;
		case XE_LOOP_TIMER:
			xe_loop_timer(loop, handle, (xe_timer*)handle -> user_data);

			break;
		case XE_LOOP_SOCKET:
			xe_loop_socket((xe_socket*)handle -> user_data, handle, res);

			break;
		case XE_LOOP_FILE:
			xe_loop_file((xe_file*)handle -> user_data, handle, res);

			break;
		case XE_NET:
			xe_net_io(handle -> user_data, handle, res);

			break;
		case XE_LOOP_IOHANDLE:
			handle -> callback(loop, handle -> user_data, handle -> u1, handle -> u2, res);

			break;
		case XE_LOOP_PROMISE: {
			xe_promise_resolve((xe_promise*)handle -> user_data, res);

			break;
		}

		default:
			xe_notreached;

			break;
	}
}

int xe_loop_run(xe_loop* loop){
	union{
		int res;
		int err;
	};

	io_uring_cqe* cqe;

	ulong packed_handle_index;

	uint handle_index;

	uint cqe_head;
	uint cqe_tail;
	uint cqe_mask;
	uint sqe_mask;

	cqe_mask = *loop -> ring.cq.kring_mask;
	cqe_head = *loop -> ring.cq.khead;
	sqe_mask = loop -> ring.sq.sqe_head;

	while(true){
		if(loop -> num_queued){
			res = xe_loop_submit(loop, 1);

			if(res >= 0){
				loop -> num_handles += res;
				loop -> num_queued -= res;
				res = 0;
			}
		}else if(loop -> num_handles){
			err = io_uring_wait_cqe(&loop -> ring, &cqe);

			if(err == -EINTR)
				err = 0;
		}else
			break;
		if(err)
			return err;
		cqe_tail = io_uring_smp_load_acquire(loop -> ring.cq.ktail);

		while(cqe_head != cqe_tail){
			cqe = &loop -> ring.cq.cqes[cqe_head & cqe_mask];
			packed_handle_index = cqe -> user_data;
			res = cqe -> res;

			handle_index = xe_handle_unpack_index(packed_handle_index);
			packed_handle_index = xe_handle_unpack_type(packed_handle_index);

			xe_assert(handle_index < loop -> capacity);

			loop -> ring.sq.sqes[cqe_head & sqe_mask].user_data = handle_index; /* link this handle to the next sqe */

			cqe_head++;
			loop -> num_handles--;

			xe_loop_process_handle(loop, (xe_handle_type)packed_handle_index, &loop -> handles[handle_index], res);
		}

		io_uring_smp_store_release(loop -> ring.cq.khead, cqe_tail);
	}

	return 0;
}