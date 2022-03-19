#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>
#include "loop.h"
#include "log.h"
#include "common.h"
#include "mem.h"
#include "error.h"
#include "net/conn.h"
#include "net/resolve.h"
#include "io/socket.h"
#include "io/file.h"

#define XE_IO_ARGS xe_ptr user_data, xe_loop_handle::xe_callback callback, ulong u1, ulong u2, xe_loop_handle_type handle_type
#define XE_IO_ARGS_PASS user_data, callback, u1, u2, handle_type

enum xe_loop_constants{
	ENTRY_COUNT = 1024, /* default sqe and cqe count */
	MAX_ENTRIES = 16777216 /* max sqe count permitted to allow packing xe_loop_handle_type and handle index into a signed int */
};

enum xe_timer_flags{
	XE_TIMER_NONE       = 0x0,
	XE_TIMER_ACTIVE     = 0x1,
	XE_TIMER_REPEAT     = 0x2,
	XE_TIMER_CALLBACK   = 0x4,
	XE_TIMER_CANCELLING = 0x8
};

/* handle pack
 * | 1 bit zero | 7bit handle_type | 24bit handle_index |
 */
static inline uint xe_handle_packed_type(xe_loop_handle_type handle_type){
	return (uint)handle_type << 24;
}

static inline uint xe_handle_unpack_type(ulong packed){
	return packed >> 24;
}

static inline uint xe_handle_unpack_index(ulong packed){
	return packed & ((1 << 24) - 1);
}

static inline bool xe_handle_invalid(xe_loop& loop, int handle){
	return handle < 0 ||
		xe_handle_unpack_index(handle) >= loop.num_capacity ||
		xe_handle_unpack_type(handle) <= XE_LOOP_HANDLE_NONE ||
		xe_handle_unpack_type(handle) >= XE_LOOP_HANDLE_LAST;
}

template<typename F>
static int xe_queue_io(xe_loop& loop, int op, XE_IO_ARGS, F init_sqe){
	xe_assert(loop.num_queued <= loop.num_capacity);
	xe_assert(loop.num_handles <= loop.num_capacity);
	xe_assert(loop.num_reserved <= loop.num_capacity);
	xe_assert(loop.num_queued + loop.num_handles + loop.num_reserved <= loop.num_capacity);

	io_uring_sqe* sqe;
	xe_loop_handle* handle;

	if(handle_type <= XE_LOOP_HANDLE_NONE || handle_type >= XE_LOOP_HANDLE_LAST)
		return XE_EINVAL;
	if(loop.num_queued + loop.num_handles + loop.num_reserved >= loop.num_capacity)
		return XE_ETOOMANYHANDLES;
	loop.num_queued++;
	sqe = &loop.ring.sq.sqes[loop.ring.sq.sqe_tail++ & loop.ring.sq.sqe_head];

	xe_assert(sqe -> user_data < loop.num_capacity); /* {sqe -> user_data} is the index of the {xe_loop_handle} that is linked to this sqe */

	/* copy io handle information */
	handle = &loop.handles[sqe -> user_data];
	handle -> user_data = user_data;
	handle -> u1 = u1;
	handle -> u2 = u2;

	if(handle_type == XE_LOOP_HANDLE_USER)
		handle -> callback = callback;
	/* copy io parameters */
	sqe -> opcode = op;
	sqe -> user_data |= xe_handle_packed_type(handle_type);

	init_sqe(sqe);

	return sqe -> user_data;
}

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

static int xe_queue_timer_internal(xe_loop& loop, xe_timer& timer){
	int res = xe_queue_io(loop, IORING_OP_TIMEOUT, &timer, null, 0, 0, XE_LOOP_HANDLE_TIMER, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> addr = (ulong)&timer.expire;
		sqe -> len = 1;
		sqe -> off = 0;
		sqe -> rw_flags = IORING_TIMEOUT_ABS;
		sqe -> buf_index = 0;
	});

	if(res >= 0)
		timer.cancel = res;
	return res;
}

static int xe_queue_timer(xe_loop& loop, xe_timer& timer){
	if(timer.flags & XE_TIMER_ACTIVE)
		return XE_EALREADY;
	if(timer.flags & XE_TIMER_CALLBACK){
		/* an sqe was reserved before callback, just mark as active */
		timer.flags |= XE_TIMER_ACTIVE;

		return 0;
	}

	int ret = xe_queue_timer_internal(loop, timer);

	if(ret >= 0){
		timer.flags |= XE_TIMER_ACTIVE;

		return 0;
	}

	return ret;
}

xe_timer::xe_timer(){
	expire = {0, 0};
	callback = null;
	start = 0;
	delay = 0;
	flags = 0;
	cancel = -1;
}

xe_loop_options::xe_loop_options(){
	capacity = 0;
	flags = 0;
	sq_thread_cpu = 0;
	pad = 0;
}

void xe_promise::resolve(int res){
	result = res;
	ready = true;
	handle = -1;

	if(waiter)
		waiter.resume();
}

xe_promise::xe_promise(){
	waiter = null;
	ready = false;
	handle = -1;
}

xe_loop::xe_loop(){
	xe_zero(&ring);

	num_handles = 0;
	num_queued = 0;
	num_reserved = 0;
	num_capacity = 0;

	io_buf = null;
	handles = null;
}

int xe_loop::init(){
	xe_loop_options options;

	return xe_loop::init_options(options);
}

int xe_loop::init_options(xe_loop_options& options){
	xe_buf io_buf;
	xe_loop_handle* handles;
	io_uring_params params;

	if(options.flags & ~(xe_loop_options::XE_FLAGS_SQPOLL | xe_loop_options::XE_FLAGS_IOPOLL | xe_loop_options::XE_FLAGS_SQAFF | xe_loop_options::XE_FLAGS_IOBUF))
		return XE_EINVAL;
	if(!options.capacity)
		options.capacity = ENTRY_COUNT;
	else if(options.capacity > MAX_ENTRIES)
		return XE_ETOOMANYHANDLES;
	io_buf = null;
	handles = null;

	if(options.flags & xe_loop_options::XE_FLAGS_IOBUF){
		io_buf = xe_alloc_aligned<byte>(0, IOBUF_SIZE_LARGE);

		if(!io_buf)
			return XE_ENOMEM;
		/* force memory allocation */
		xe_zero((byte*)io_buf, IOBUF_SIZE_LARGE);
	}

	xe_zero(&params);

	params.cq_entries = options.capacity;
	params.flags = IORING_SETUP_CQSIZE;

	if(options.flags & xe_loop_options::XE_FLAGS_IOPOLL)
		params.flags |= IORING_SETUP_IOPOLL;
	if(options.flags & xe_loop_options::XE_FLAGS_SQPOLL)
		params.flags |= IORING_SETUP_SQPOLL;
	if(options.flags & xe_loop_options::XE_FLAGS_SQAFF){
		params.flags |= IORING_SETUP_SQ_AFF;
		params.sq_thread_cpu = options.sq_thread_cpu;
	}

	int err = io_uring_queue_init_params(options.capacity, &ring, &params);

	if(err){
		if(io_buf)
			xe_dealloc(io_buf);
		return err;
	}

	handles = xe_alloc_aligned<xe_loop_handle>(0, params.sq_entries);

	if(!handles){
		io_uring_queue_exit(&ring);

		if(io_buf)
			xe_dealloc(io_buf);
		return XE_ENOMEM;
	}

	/* force memory allocation */
	xe_zero(handles, params.sq_entries);

	for(uint i = 0; i < params.sq_entries; i++){
		ring.sq.sqes[i].user_data = i; /* link each sqe to an {xe_loop_handle} */
		ring.sq.array[i] = i; /* fill the sqe array */
	}

	num_capacity = params.sq_entries;
	ring.sq.sqe_head = *ring.sq.kring_mask; /* sqe_head will be used as the sqe mask */

	this -> handles = handles;
	this -> io_buf = io_buf;

	xe_log_trace("xe_loop", this, "init(), %u entries", num_capacity);

	return 0;
}

void xe_loop::destroy(){
	io_uring_queue_exit(&ring);
	xe_dealloc(handles);
	xe_dealloc(io_buf);

	xe_log_trace("xe_loop", this, "destroy()");
}

int xe_loop::timer_ms(xe_timer& timer, ulong millis, bool repeat){
	return timer_ns(timer, millis * (ulong)1e6, repeat);
}

int xe_loop::timer_ns(xe_timer& timer, ulong nanos, bool repeat){
	timer.start = xe_time_ns();
	timer.delay = nanos;
	timer.expire.tv_nsec = timer.start + nanos;

	if(repeat)
		timer.flags |= XE_TIMER_REPEAT;
	else
		timer.flags &= ~XE_TIMER_REPEAT;
	return xe_queue_timer(*this, timer);
}

int xe_loop::timer_cancel(xe_timer& timer){
	if(timer.flags & XE_TIMER_CALLBACK){
		timer.flags &= ~XE_TIMER_REPEAT;

		return 0;
	}

	if(!(timer.flags & XE_TIMER_ACTIVE))
		return XE_EINVAL;
	if(timer.flags & XE_TIMER_CANCELLING)
		return XE_EALREADY;
	xe_assert(!xe_handle_invalid(*this, timer.cancel));

	int ret = xe_queue_io(*this, IORING_OP_TIMEOUT_REMOVE, null, null, 0, 0, XE_LOOP_HANDLE_DISCARD, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> addr = (ulong)timer.cancel;
		sqe -> len = 0;
		sqe -> rw_flags = 0;
		sqe -> buf_index = 0;
	});

	if(ret >= 0){
		timer.flags &= ~XE_TIMER_REPEAT;
		timer.flags |= XE_TIMER_CANCELLING;

		return 0;
	}

	return ret;
}

int xe_loop::nop(XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_NOP, XE_IO_ARGS_PASS, [](io_uring_sqe* sqe){
		sqe -> flags = 0;
	});
}

int xe_loop::openat(int fd, xe_cstr path, uint flags, mode_t mode, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_OPENAT, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> fd = fd;
		sqe -> addr = (ulong)path;
		sqe -> len = mode;
		sqe -> rw_flags = flags;
		sqe -> buf_index = 0;
	});
}

int xe_loop::openat2(int fd, xe_cstr path, struct open_how* how, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_OPENAT2, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> fd = fd;
		sqe -> addr = (ulong)path;
		sqe -> len = sizeof(*how);
		sqe -> off = (ulong)how;
		sqe -> buf_index = 0;
	});
}

int xe_loop::read(int fd, xe_buf buf, uint len, ulong offset, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_READ, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, buf, len, offset, 0, 0);
	});
}

int xe_loop::write(int fd, xe_buf buf, uint len, ulong offset, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_WRITE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, buf, len, offset, 0, 0);
	});
}

int xe_loop::readv(int fd, iovec* vecs, uint vlen, ulong offset, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_READV, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, vecs, vlen, offset, 0, 0);
	});
}

int xe_loop::writev(int fd, iovec* vecs, uint vlen, ulong offset, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_WRITEV, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, vecs, vlen, offset, 0, 0);
	});
}

int xe_loop::read_fixed(int fd, xe_buf buf, uint len, ulong offset, uint buf_index, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_READ_FIXED, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, buf, len, offset, 0, buf_index);
	});
}

int xe_loop::write_fixed(int fd, xe_buf buf, uint len, ulong offset, uint buf_index, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_WRITE_FIXED, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, buf, len, offset, 0, buf_index);
	});
}

int xe_loop::fsync(int fd, uint flags, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_FSYNC, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> fd = fd;
		sqe -> addr = 0;
		sqe -> rw_flags = flags;
		sqe -> buf_index = 0;
	});
}

int xe_loop::files_update(int* fds, uint len, uint offset, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_FILES_UPDATE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> addr = (ulong)fds;
		sqe -> len = len;
		sqe -> off = offset;
		sqe -> rw_flags = 0;
	});
}

int xe_loop::fallocate(int fd, int mode, ulong offset, ulong len, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_FALLOCATE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, (xe_ptr)len, mode, offset, 0, 0);
	});
}

int xe_loop::fadvise(int fd, ulong offset, ulong len, uint advice, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_FADVISE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, null, len, offset, advice, 0);
	});
}

int xe_loop::sync_file_range(int fd, uint len, ulong offset, uint flags, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_SYNC_FILE_RANGE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, null, len, offset, flags, 0);
	});
}

int xe_loop::statx(int fd, xe_cstr path, uint flags, uint mask, struct statx* statx, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_STATX, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, path, mask, (ulong)statx, flags, 0);
	});
}

int xe_loop::renameat(int old_fd, xe_cstr old_path, int new_fd, xe_cstr new_path, uint flags, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_RENAMEAT, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> fd = old_fd;
		sqe -> addr = (ulong)old_path;
		sqe -> len = new_fd;
		sqe -> off = (ulong)new_path;
		sqe -> rw_flags = flags;
	});
}

int xe_loop::unlinkat(int fd, xe_cstr path, uint flags, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_UNLINKAT, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> fd = fd;
		sqe -> addr = (ulong)path;
		sqe -> rw_flags = flags;
	});
}

int xe_loop::splice(int fd_in, ulong off_in, int fd_out, ulong off_out, uint len, uint flags, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_SPLICE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_pipe(sqe, 0, fd_in, off_in, fd_out, off_out, len, flags);
	});
}

int xe_loop::tee(int fd_in, int fd_out, uint len, uint flags, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_TEE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_pipe(sqe, 0, fd_in, 0, fd_out, 0, len, flags);
	});
}

int xe_loop::connect(int fd, sockaddr* addr, socklen_t addrlen, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_CONNECT, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, addr, 0, addrlen, 0, 0);
	});
}

int xe_loop::accept(int fd, sockaddr* addr, socklen_t* addrlen, uint flags, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_ACCEPT, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, addr, 0, (ulong)addrlen, flags, 0);
	});
}

int xe_loop::recv(int fd, xe_buf buf, uint len, uint flags, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_RECV, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_socket_rw(sqe, 0, fd, buf, len, flags);
	});
}

int xe_loop::send(int fd, xe_buf buf, uint len, uint flags, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_SEND, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_socket_rw(sqe, 0, fd, buf, len, flags);
	});
}

int xe_loop::recvmsg(int fd, struct msghdr* msg, uint flags, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_RECVMSG, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_socket_rw(sqe, 0, fd, msg, 1, flags);
	});
}

int xe_loop::sendmsg(int fd, struct msghdr* msg, uint flags, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_SENDMSG, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_socket_rw(sqe, 0, fd, msg, 1, flags);
	});
}

int xe_loop::poll(int fd, uint mask, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_POLL_ADD, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, null, 0, 0, mask, 0);
	});
}

int xe_loop::poll_cancel(int hd, XE_IO_ARGS){
	if(xe_handle_invalid(*this, hd))
		return XE_EINVAL;
	return xe_queue_io(*this, IORING_OP_POLL_REMOVE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> addr = (ulong)hd;
		sqe -> len = 0;
		sqe -> off = 0;
		sqe -> rw_flags = 0;
		sqe -> buf_index = 0;
	});
}

int xe_loop::madvise(xe_ptr addr, ulong len, uint advice, XE_IO_ARGS){
	return xe_queue_io(*this, IORING_OP_MADVISE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> addr = (ulong)addr;
		sqe -> len = len;
		sqe -> off = 0;
		sqe -> rw_flags = advice;
		sqe -> buf_index = 0;
	});
}

int xe_loop::cancel(int hd, uint flags, XE_IO_ARGS){
	if(xe_handle_invalid(*this, hd))
		return XE_EINVAL;
	return xe_queue_io(*this, IORING_OP_ASYNC_CANCEL, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> addr = (ulong)hd;
		sqe -> len = 0;
		sqe -> off = 0;
		sqe -> rw_flags = flags;
	});
}

int xe_loop::modify_handle(int hd, xe_ptr user_data, xe_loop_handle::xe_callback callback, ulong u1, ulong u2){
	if(xe_handle_invalid(*this, hd))
		return XE_EINVAL;
	xe_loop_handle* handle = &handles[xe_handle_unpack_index(hd)];

	handle -> u1 = u1;
	handle -> u2 = u2;
	handle -> user_data = user_data;
	handle -> callback = callback;

	return 0;
}

#define xe_promise_start(op, ...)										\
	xe_promise promise;													\
																		\
	int ret = op(__VA_ARGS__ &promise, null, 0, 0, XE_LOOP_HANDLE_PROMISE);	\
																		\
	if(ret < 0)															\
		promise.resolve(ret);											\
	else																\
		promise.handle = ret;											\
	return promise;
xe_promise xe_loop::nop(){
	xe_promise_start(nop)
}

xe_promise xe_loop::openat(int fd, xe_cstr path, uint flags, mode_t mode){
	xe_promise_start(openat, fd, path, flags, mode, )
}

xe_promise xe_loop::openat2(int fd, xe_cstr path, struct open_how* how){
	xe_promise_start(openat2, fd, path, how, )
}

xe_promise xe_loop::read(int fd, xe_buf buf, uint len, ulong offset){
	xe_promise_start(read, fd, buf, len, offset, )
}

xe_promise xe_loop::write(int fd, xe_buf buf, uint len, ulong offset){
	xe_promise_start(write, fd, buf, len, offset, )
}

xe_promise xe_loop::readv(int fd, iovec* iovecs, uint vlen, ulong offset){
	xe_promise_start(readv, fd, iovecs, vlen, offset, )
}

xe_promise xe_loop::writev(int fd, iovec* iovecs, uint vlen, ulong offset){
	xe_promise_start(writev, fd, iovecs, vlen, offset, )
}

xe_promise xe_loop::read_fixed(int fd, xe_buf buf, uint len, ulong offset, uint buf_index){
	xe_promise_start(read_fixed, fd, buf, len, offset, buf_index, )
}

xe_promise xe_loop::write_fixed(int fd, xe_buf buf, uint len, ulong offset, uint buf_index){
	xe_promise_start(write_fixed, fd, buf, len, offset, buf_index, )
}

xe_promise xe_loop::fsync(int fd, uint flags){
	xe_promise_start(fsync, fd, flags, )
}

xe_promise xe_loop::fallocate(int fd, int mode, ulong offset, ulong len){
	xe_promise_start(fallocate, fd, mode, offset, len, )
}

xe_promise xe_loop::fadvise(int fd, ulong offset, ulong len, uint advice){
	xe_promise_start(fadvise, fd, offset, len, advice, )
}

xe_promise xe_loop::madvise(xe_ptr addr, ulong len, uint advice){
	xe_promise_start(madvise, addr, len, advice, )
}

xe_promise xe_loop::sync_file_range(int fd, uint len, ulong offset, uint flags){
	xe_promise_start(sync_file_range, fd, len, offset, flags, )
}

xe_promise xe_loop::statx(int fd, xe_cstr path, uint flags, uint mask, struct statx* stat){
	xe_promise_start(statx, fd, path, flags, mask, stat, )
}

xe_promise xe_loop::renameat(int old_fd, xe_cstr old_path, int new_fd, xe_cstr new_path, uint flags){
	xe_promise_start(renameat, old_fd, old_path, new_fd, new_path, flags, )
}

xe_promise xe_loop::unlinkat(int fd, xe_cstr path, uint flags){
	xe_promise_start(unlinkat, fd, path, flags, )
}

xe_promise xe_loop::splice(int fd_in, ulong off_in, int fd_out, ulong off_out, uint len, uint flags){
	xe_promise_start(splice, fd_in, off_in, fd_out, off_out, len, flags, )
}

xe_promise xe_loop::tee(int fd_in, int fd_out, uint len, uint flags){
	xe_promise_start(tee, fd_in, fd_out, len, flags, )
}

xe_promise xe_loop::connect(int fd, sockaddr* addr, socklen_t addrlen){
	xe_promise_start(connect, fd, addr, addrlen, )
}

xe_promise xe_loop::accept(int fd, sockaddr* addr, socklen_t* addrlen, uint flags){
	xe_promise_start(accept, fd, addr, addrlen, flags, )
}

xe_promise xe_loop::recv(int fd, xe_buf buf, uint len, uint flags){
	xe_promise_start(recv, fd, buf, len, flags, )
}

xe_promise xe_loop::send(int fd, xe_buf buf, uint len, uint flags){
	xe_promise_start(send, fd, buf, len, flags, )
}

xe_promise xe_loop::recvmsg(int fd, struct msghdr* msg, uint flags){
	xe_promise_start(recvmsg, fd, msg, flags, )
}

xe_promise xe_loop::sendmsg(int fd, struct msghdr* msg, uint flags){
	xe_promise_start(sendmsg, fd, msg, flags, )
}

xe_promise xe_loop::poll(int fd, uint mask){
	xe_promise_start(poll, fd, mask, )
}

xe_promise xe_loop::poll_cancel(int handle){
	xe_promise_start(poll_cancel, handle, )
}

xe_promise xe_loop::cancel(int handle, uint flags){
	xe_promise_start(cancel, handle, flags, )
}

xe_promise xe_loop::files_update(int* fds, uint len, uint offset){
	xe_promise_start(files_update, fds, len, offset, )
}

xe_buf xe_loop::iobuf() const{
	return io_buf;
}

xe_buf xe_loop::iobuf_large() const{
	return io_buf;
}

uint xe_loop::remain(){
	return num_capacity - num_handles - num_queued - num_reserved;
}

uint xe_loop::capacity(){
	return num_capacity;
}

bool xe_loop::reserve(uint count){
	if(remain() >= count){
		num_reserved += count;

		return true;
	}

	return false;
}

void xe_loop::release(uint count){
	if(num_reserved < count)
		xe_stop("invalid release");
	num_reserved -= count;
}

static void xe_loop_timer(xe_loop& loop, xe_timer& timer){
	ulong time_now, time_align;

	loop.reserve(1); /* reserve an sqe for the timer */
	timer.flags &= ~(XE_TIMER_ACTIVE | XE_TIMER_CANCELLING);
	timer.flags |= XE_TIMER_CALLBACK;
	timer.callback(loop, timer);
	timer.flags &= ~XE_TIMER_CALLBACK;
	loop.release(1);

	if(timer.flags & XE_TIMER_ACTIVE)
		/* timer was set in callback */
		xe_queue_timer_internal(loop, timer);
	else if(timer.flags & XE_TIMER_REPEAT){
		time_now = xe_time_ns();

		if(timer.delay > 0){
			time_align = (time_now - timer.start - 1) % timer.delay;
			timer.expire.tv_nsec = time_now + timer.delay - time_align - 1;
			timer.start = timer.expire.tv_nsec - timer.delay;
		}else{
			timer.start = time_now;
			timer.expire.tv_nsec = timer.start;
		}

		xe_queue_timer(loop, timer);

		timer.flags |= XE_TIMER_ACTIVE;
	}
}

static int xe_loop_enter(xe_loop& loop, uint submit, uint wait, uint flags){
	return syscall(__NR_io_uring_enter, loop.ring.ring_fd, submit, wait, flags, null, _NSIG / 8);
}

static int xe_loop_submit(xe_loop& loop, uint wait){
	uint flags = 0;
	uint submit = loop.ring.sq.sqe_tail - *loop.ring.sq.khead;

	int ret;

	io_uring_smp_store_release(loop.ring.sq.ktail, loop.ring.sq.sqe_tail);

	if(!(loop.ring.flags & IORING_SETUP_IOPOLL)) [[likely]]
		goto enter;
	if(IO_URING_READ_ONCE(*loop.ring.sq.kflags) & IORING_SQ_NEED_WAKEUP) [[unlikely]] {
		flags |= IORING_ENTER_SQ_WAKEUP;

		goto enter;
	}

	if(wait)
		goto enter;
	return submit;

	enter:

	if(wait || (loop.ring.flags & IORING_SETUP_IOPOLL))
		flags |= IORING_ENTER_GETEVENTS;
	ret = xe_loop_enter(loop, submit, wait, flags);

	if(ret < 0)
		return xe_errno();
	return ret;
}

static int xe_loop_waitsingle(xe_loop& loop){
	int ret = xe_loop_enter(loop, 0, 1, IORING_ENTER_GETEVENTS);

	if(ret < 0){
		ret = xe_errno();

		if(ret == XE_EINTR)
			return 0;
		return ret;
	}

	return 0;
}

int xe_loop::run(){
	xe_loop_handle* handle;

	int res;

	uint handle_index;
	ulong packed_handle_index;

	uint cqe_head;
	uint cqe_tail;
	uint cqe_mask;
	uint sqe_mask;

	cqe_mask = *ring.cq.kring_mask;
	cqe_head = *ring.cq.khead;
	sqe_mask = ring.sq.sqe_head;

	while(true){
		if(num_queued){
			res = xe_loop_submit(*this, 1);

			if(res >= 0){
				xe_log_trace("xe_loop", this, "queued %u handles", res);

				num_handles += res;
				num_queued -= res;
				res = 0;
			}

			cqe_tail = io_uring_smp_load_acquire(ring.cq.ktail);
		}else if(num_handles){
			cqe_tail = io_uring_smp_load_acquire(ring.cq.ktail);
			res = 0;

			if(cqe_head == cqe_tail){
				res = xe_loop_waitsingle(*this);
				cqe_tail = io_uring_smp_load_acquire(ring.cq.ktail);
			}
		}else{
			/* nothing else to do */
			break;
		}

		if(res)
			return res;
		xe_log_trace("xe_loop", this, "processing %u handles", cqe_tail - cqe_head);

		while(cqe_head != cqe_tail){
			packed_handle_index = ring.cq.cqes[cqe_head & cqe_mask].user_data;
			res = ring.cq.cqes[cqe_head & cqe_mask].res;

			handle_index = xe_handle_unpack_index(packed_handle_index);
			packed_handle_index = xe_handle_unpack_type(packed_handle_index);

			xe_assert(handle_index < num_capacity);

			ring.sq.sqes[cqe_head & sqe_mask].user_data = handle_index; /* link this handle to the next sqe */
			cqe_head++;
			num_handles--;
			handle = &handles[handle_index];

			switch(packed_handle_index){
				case XE_LOOP_HANDLE_DISCARD:
					break;
				case XE_LOOP_HANDLE_TIMER:
					xe_loop_timer(*this, *(xe_timer*)handle -> user_data);

					break;
				case XE_LOOP_HANDLE_SOCKET:
					xe_socket::io(*handle, res);

					break;
				case XE_LOOP_HANDLE_FILE:
					xe_file::io(*handle, res);

					break;
				case XE_NET_CONNECTION:
					xe_net::xe_net_ctx::io(*handle, res);

					break;
				case XE_NET_RESOLVER:
					xe_net::xe_resolve::io(*handle, res);

					break;
				case XE_LOOP_HANDLE_USER:
					if(!handle -> callback)
						break;
					handle -> callback(*this, handle -> user_data, handle -> u1, handle -> u2, res);

					break;
				case XE_LOOP_HANDLE_PROMISE: {
					xe_promise* promise = (xe_promise*)handle -> user_data;

					promise -> resolve(res);

					break;
				}

				default:
					xe_notreached();

					break;
			}
		}

		io_uring_smp_store_release(ring.cq.khead, cqe_tail);
	}

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