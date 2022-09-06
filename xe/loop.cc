#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>
#include "loop.h"
#include "xutil/log.h"
#include "clock.h"
#include "xstd/types.h"
#include "xutil/util.h"
#include "xutil/mem.h"
#include "io/socket.h"
#include "io/file.h"
#include "xurl/ctx.h"
#include "error.h"

#define XE_IO_ARGS xe_ptr user_data, xe_loop_handle::xe_callback callback, ulong u1, ulong u2, xe_loop_handle_type handle_type
#define XE_IO_ARGS_PASS user_data, callback, u1, u2, handle_type

enum{
	ENTRY_COUNT = 1024, /* default sqe and cqe count */
	MAX_ENTRIES = 16777216 /* max sqe count permitted to allow packing xe_loop_handle_type and handle index into a signed int */
};

enum xe_handle_constants{
	INVALID_HANDLE = -1,
	READY_HANDLE = -2
};

/* handle pack
 * | 1 bit zero | 7 bit handle_type | 24 bit handle_index |
 */
static inline uint xe_handle_pack(xe_loop_handle_type handle_type, uint handle_index){
	return ((uint)handle_type << 24) | handle_index;
}

static inline uint xe_handle_unpack_type(ulong packed){
	return packed >> 24;
}

static inline uint xe_handle_unpack_index(ulong packed){
	return packed & ((1 << 24) - 1);
}

inline bool xe_loop::handle_invalid(int handle){
	return handle <= INVALID_HANDLE ||
		xe_handle_unpack_index(handle) >= num_capacity ||
		xe_handle_unpack_type(handle) <= XE_LOOP_HANDLE_NONE ||
		xe_handle_unpack_type(handle) >= XE_LOOP_HANDLE_LAST;
}

template<typename F>
int xe_loop::queue_io(int op, XE_IO_ARGS, F init_sqe){
	xe_assert(num_queued <= num_capacity);
	xe_assert(num_handles <= num_capacity);
	xe_assert(num_reserved <= num_capacity);
	xe_assert(num_queued + num_handles + num_reserved <= num_capacity);

	io_uring_sqe* sqe;
	xe_loop_handle* handle;

	if(handle_type >= XE_LOOP_HANDLE_LAST)
		return XE_EINVAL;
	if(num_queued + num_handles + num_reserved >= num_capacity)
		return XE_TOOMANYHANDLES;
	num_queued++;
	sqe = &ring.sq.sqes[ring.sq.sqe_tail++ & ring.sq.sqe_head];

	xe_assert(sqe -> user_data < num_capacity); /* {sqe -> user_data} is the index of the {xe_loop_handle} that is linked to this sqe */

	/* copy io handle information */
	handle = &handles[sqe -> user_data];
	handle -> user_data = user_data;
	handle -> u1 = u1;
	handle -> u2 = u2;

	if(handle_type == XE_LOOP_HANDLE_USER)
		handle -> callback = callback;
	/* copy io parameters */
	sqe -> opcode = op;
	sqe -> user_data = xe_handle_pack(handle_type, sqe -> user_data);

	init_sqe(sqe);

	return sqe -> user_data;
}

static inline void xe_sqe_rw(io_uring_sqe* sqe, uint flags, int fd, xe_cptr addr, uint len, ulong offset, uint rw_flags, uint buf_index){
	sqe -> flags = flags;
	sqe -> fd = fd;
	sqe -> addr = (ulong)addr;
	sqe -> len = len;
	sqe -> off = offset;
	sqe -> rw_flags = rw_flags;
	sqe -> buf_index = buf_index;
}

static inline void xe_sqe_socket_rw(io_uring_sqe* sqe, uint flags, int fd, xe_cptr addr, uint len, uint rw_flags){
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

xe_timer::xe_timer(){
	expire.key = 0;
	delay = 0;
	active_ = false;
	repeat_ = false;
	align_ = false;
	in_callback = false;
}

bool xe_timer::active() const{
	return active_;
}

bool xe_timer::repeat() const{
	return repeat_;
}

bool xe_timer::align() const{
	return align_;
}

bool xe_timer::passive() const{
	return passive_;
}

xe_loop_options::xe_loop_options(){
	capacity = 0;
	sq_thread_cpu = 0;
	flag_sqpoll = 0;
	flag_iopoll = 0;
	flag_sqaff = 0;
	flag_iobuf = 0;
}

xe_promise::xe_promise(){
	waiter = null;
}

bool xe_promise::await_ready(){
	return handle_ == READY_HANDLE;
}

void xe_promise::await_suspend(std::coroutine_handle<> handle){
	waiter = handle;
}

int xe_promise::await_resume(){
	return result_;
}

int xe_promise::handle(){
	return handle_;
}

xe_loop::xe_loop(){
	xe_zero(&ring);

	num_handles = 0;
	num_queued = 0;
	num_reserved = 0;
	num_capacity = 0;

	io_buf = null;
	handles = null;

	active_timers = 0;
}

int xe_loop::init(){
	xe_loop_options options;

	return xe_loop::init_options(options);
}

int xe_loop::init_options(xe_loop_options& options){
	xe_loop_handle* handles_;
	io_uring_params params;
	byte* io_buf_;
	int err;

	if(!options.capacity)
		options.capacity = ENTRY_COUNT;
	else if(options.capacity > MAX_ENTRIES)
		return XE_EINVAL;
	io_buf_ = null;
	handles_ = null;

	if(options.flag_iobuf){
		io_buf_ = xe_alloc_aligned<byte>(0, XE_LOOP_IOBUF_SIZE_LARGE);

		if(!io_buf_) return XE_ENOMEM;
		/* force memory allocation */
		xe_zero(io_buf_, XE_LOOP_IOBUF_SIZE_LARGE);
	}

	xe_zero(&params);

	params.cq_entries = options.capacity;
	params.flags = IORING_SETUP_CQSIZE;

	if(options.flag_iopoll)
		params.flags |= IORING_SETUP_IOPOLL;
	if(options.flag_sqpoll)
		params.flags |= IORING_SETUP_SQPOLL;
	if(options.flag_sqaff){
		params.flags |= IORING_SETUP_SQ_AFF;
		params.sq_thread_cpu = options.sq_thread_cpu;
	}

	err = io_uring_queue_init_params(options.capacity, &ring, &params);

	if(err){
		xe_dealloc(io_buf_);

		return err;
	}

	handles_ = xe_alloc_aligned<xe_loop_handle>(0, params.sq_entries);

	if(!handles_){
		io_uring_queue_exit(&ring);
		xe_dealloc(io_buf_);

		return XE_ENOMEM;
	}

	/* force memory allocation */
	xe_zero(handles_, params.sq_entries);

	for(uint i = 0; i < params.sq_entries; i++){
		ring.sq.sqes[i].user_data = i; /* link each sqe to an {xe_loop_handle} */
		ring.sq.array[i] = i; /* fill the sqe array */
	}

	num_capacity = params.sq_entries;
	ring.sq.sqe_head = *ring.sq.kring_mask; /* sqe_head will be used as the sqe mask */

	handles = handles_;
	io_buf = io_buf_;

	xe_log_trace(this, "init(), %u entries", num_capacity);

	return 0;
}

void xe_loop::close(){
	io_uring_queue_exit(&ring);
	xe_dealloc(handles);
	xe_dealloc(io_buf);

	xe_log_trace(this, "close()");
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

int xe_loop::timer_cancel(xe_timer& timer){
	if(timer.in_callback){
		timer.repeat_ = false; /* won't be queued again */

		return 0;
	}

	if(!timer.active_)
		return XE_ENOENT;
	timer.repeat_ = false;
	timer.active_= false;

	if(!timer.passive_)
		active_timers--;
	timers.erase(timer.expire);

	return 0;
}

int xe_loop::nop(XE_IO_ARGS){
	return queue_io(IORING_OP_NOP, XE_IO_ARGS_PASS, [](io_uring_sqe* sqe){
		sqe -> flags = 0;
	});
}

int xe_loop::openat(int fd, xe_cstr path, uint flags, mode_t mode, XE_IO_ARGS){
	return queue_io(IORING_OP_OPENAT, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> fd = fd;
		sqe -> addr = (ulong)path;
		sqe -> len = mode;
		sqe -> rw_flags = flags;
		sqe -> buf_index = 0;
	});
}

int xe_loop::openat2(int fd, xe_cstr path, struct open_how* how, XE_IO_ARGS){
	return queue_io(IORING_OP_OPENAT2, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> fd = fd;
		sqe -> addr = (ulong)path;
		sqe -> len = sizeof(*how);
		sqe -> off = (ulong)how;
		sqe -> buf_index = 0;
	});
}

int xe_loop::read(int fd, xe_ptr buf, uint len, ulong offset, XE_IO_ARGS){
	return queue_io(IORING_OP_READ, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, buf, len, offset, 0, 0);
	});
}

int xe_loop::write(int fd, xe_cptr buf, uint len, ulong offset, XE_IO_ARGS){
	return queue_io(IORING_OP_WRITE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, buf, len, offset, 0, 0);
	});
}

int xe_loop::readv(int fd, iovec* vecs, uint vlen, ulong offset, XE_IO_ARGS){
	return queue_io(IORING_OP_READV, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, vecs, vlen, offset, 0, 0);
	});
}

int xe_loop::writev(int fd, iovec* vecs, uint vlen, ulong offset, XE_IO_ARGS){
	return queue_io(IORING_OP_WRITEV, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, vecs, vlen, offset, 0, 0);
	});
}

int xe_loop::read_fixed(int fd, xe_ptr buf, uint len, ulong offset, uint buf_index, XE_IO_ARGS){
	return queue_io(IORING_OP_READ_FIXED, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, buf, len, offset, 0, buf_index);
	});
}

int xe_loop::write_fixed(int fd, xe_cptr buf, uint len, ulong offset, uint buf_index, XE_IO_ARGS){
	return queue_io(IORING_OP_WRITE_FIXED, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, buf, len, offset, 0, buf_index);
	});
}

int xe_loop::fsync(int fd, uint flags, XE_IO_ARGS){
	return queue_io(IORING_OP_FSYNC, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> fd = fd;
		sqe -> addr = 0;
		sqe -> rw_flags = flags;
		sqe -> buf_index = 0;
	});
}

int xe_loop::files_update(int* fds, uint len, uint offset, XE_IO_ARGS){
	return queue_io(IORING_OP_FILES_UPDATE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> addr = (ulong)fds;
		sqe -> len = len;
		sqe -> off = offset;
		sqe -> rw_flags = 0;
	});
}

int xe_loop::fallocate(int fd, int mode, ulong offset, ulong len, XE_IO_ARGS){
	return queue_io(IORING_OP_FALLOCATE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, (xe_ptr)len, mode, offset, 0, 0);
	});
}

int xe_loop::fadvise(int fd, ulong offset, ulong len, uint advice, XE_IO_ARGS){
	return queue_io(IORING_OP_FADVISE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, null, len, offset, advice, 0);
	});
}

int xe_loop::sync_file_range(int fd, uint len, ulong offset, uint flags, XE_IO_ARGS){
	return queue_io(IORING_OP_SYNC_FILE_RANGE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, null, len, offset, flags, 0);
	});
}

int xe_loop::statx(int fd, xe_cstr path, uint flags, uint mask, struct statx* statx, XE_IO_ARGS){
	return queue_io(IORING_OP_STATX, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, path, mask, (ulong)statx, flags, 0);
	});
}

int xe_loop::renameat(int old_fd, xe_cstr old_path, int new_fd, xe_cstr new_path, uint flags, XE_IO_ARGS){
	return queue_io(IORING_OP_RENAMEAT, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> fd = old_fd;
		sqe -> addr = (ulong)old_path;
		sqe -> len = new_fd;
		sqe -> off = (ulong)new_path;
		sqe -> rw_flags = flags;
	});
}

int xe_loop::unlinkat(int fd, xe_cstr path, uint flags, XE_IO_ARGS){
	return queue_io(IORING_OP_UNLINKAT, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> fd = fd;
		sqe -> addr = (ulong)path;
		sqe -> rw_flags = flags;
	});
}

int xe_loop::splice(int fd_in, ulong off_in, int fd_out, ulong off_out, uint len, uint flags, XE_IO_ARGS){
	return queue_io(IORING_OP_SPLICE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_pipe(sqe, 0, fd_in, off_in, fd_out, off_out, len, flags);
	});
}

int xe_loop::tee(int fd_in, int fd_out, uint len, uint flags, XE_IO_ARGS){
	return queue_io(IORING_OP_TEE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_pipe(sqe, 0, fd_in, 0, fd_out, 0, len, flags);
	});
}

int xe_loop::connect(int fd, sockaddr* addr, socklen_t addrlen, XE_IO_ARGS){
	return queue_io(IORING_OP_CONNECT, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, addr, 0, addrlen, 0, 0);
	});
}

int xe_loop::accept(int fd, sockaddr* addr, socklen_t* addrlen, uint flags, XE_IO_ARGS){
	return queue_io(IORING_OP_ACCEPT, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, addr, 0, (ulong)addrlen, flags, 0);
	});
}

int xe_loop::recv(int fd, xe_ptr buf, uint len, uint flags, XE_IO_ARGS){
	return queue_io(IORING_OP_RECV, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_socket_rw(sqe, 0, fd, buf, len, flags);
	});
}

int xe_loop::send(int fd, xe_cptr buf, uint len, uint flags, XE_IO_ARGS){
	return queue_io(IORING_OP_SEND, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_socket_rw(sqe, 0, fd, buf, len, flags);
	});
}

int xe_loop::recvmsg(int fd, struct msghdr* msg, uint flags, XE_IO_ARGS){
	return queue_io(IORING_OP_RECVMSG, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_socket_rw(sqe, 0, fd, msg, 1, flags);
	});
}

int xe_loop::sendmsg(int fd, struct msghdr* msg, uint flags, XE_IO_ARGS){
	return queue_io(IORING_OP_SENDMSG, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_socket_rw(sqe, 0, fd, msg, 1, flags);
	});
}

int xe_loop::poll(int fd, uint mask, XE_IO_ARGS){
	return queue_io(IORING_OP_POLL_ADD, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		xe_sqe_rw(sqe, 0, fd, null, 0, 0, mask, 0);
	});
}

int xe_loop::poll_cancel(int hd, XE_IO_ARGS){
	if(handle_invalid(hd))
		return XE_EINVAL;
	return queue_io(IORING_OP_POLL_REMOVE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> addr = (ulong)hd;
		sqe -> len = 0;
		sqe -> off = 0;
		sqe -> rw_flags = 0;
		sqe -> buf_index = 0;
	});
}

int xe_loop::madvise(xe_ptr addr, ulong len, uint advice, XE_IO_ARGS){
	return queue_io(IORING_OP_MADVISE, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> addr = (ulong)addr;
		sqe -> len = len;
		sqe -> off = 0;
		sqe -> rw_flags = advice;
		sqe -> buf_index = 0;
	});
}

int xe_loop::cancel(int hd, uint flags, XE_IO_ARGS){
	if(handle_invalid(hd))
		return XE_EINVAL;
	return queue_io(IORING_OP_ASYNC_CANCEL, XE_IO_ARGS_PASS, [&](io_uring_sqe* sqe){
		sqe -> flags = 0;
		sqe -> addr = (ulong)hd;
		sqe -> len = 0;
		sqe -> off = 0;
		sqe -> rw_flags = flags;
	});
}

int xe_loop::modify_handle(int hd, xe_ptr user_data, xe_loop_handle::xe_callback callback, ulong u1, ulong u2){
	if(handle_invalid(hd))
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
	int res = op(__VA_ARGS__ &promise, null, 0, 0, XE_LOOP_HANDLE_PROMISE);	\
																		\
	if(res < 0){														\
		promise.result_ = res;											\
		promise.handle_ = READY_HANDLE;									\
	}else{																\
		promise.handle_ = res;											\
		promise.result_ = 0;											\
	}																	\
																		\
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

xe_promise xe_loop::read(int fd, xe_ptr buf, uint len, ulong offset){
	xe_promise_start(read, fd, buf, len, offset, )
}

xe_promise xe_loop::write(int fd, xe_cptr buf, uint len, ulong offset){
	xe_promise_start(write, fd, buf, len, offset, )
}

xe_promise xe_loop::readv(int fd, iovec* iovecs, uint vlen, ulong offset){
	xe_promise_start(readv, fd, iovecs, vlen, offset, )
}

xe_promise xe_loop::writev(int fd, iovec* iovecs, uint vlen, ulong offset){
	xe_promise_start(writev, fd, iovecs, vlen, offset, )
}

xe_promise xe_loop::read_fixed(int fd, xe_ptr buf, uint len, ulong offset, uint buf_index){
	xe_promise_start(read_fixed, fd, buf, len, offset, buf_index, )
}

xe_promise xe_loop::write_fixed(int fd, xe_cptr buf, uint len, ulong offset, uint buf_index){
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

xe_promise xe_loop::recv(int fd, xe_ptr buf, uint len, uint flags){
	xe_promise_start(recv, fd, buf, len, flags, )
}

xe_promise xe_loop::send(int fd, xe_cptr buf, uint len, uint flags){
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

xe_ptr xe_loop::iobuf() const{
	return io_buf;
}

xe_ptr xe_loop::iobuf_large() const{
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
	xe_assert(num_reserved >= count);

	num_reserved -= count;
}

void xe_loop::run_timer(xe_timer& timer, ulong now){
	ulong align, delay;
	int ret;

	if(!timer.passive_)
		active_timers--;
	timer.active_ = false;
	timer.in_callback = true;
	ret = timer.callback(*this, timer);

	if(ret){
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
	else if(delay > 0){
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

int xe_loop::enter(uint submit, uint wait, uint flags, ulong timeout){
	__kernel_timespec ts;
	io_uring_getevents_arg args;
	xe_ptr arg;
	size_t sz;

	if(timeout){
		flags |= IORING_ENTER_EXT_ARG;
		args.sigmask = 0;
		args.sigmask_sz = _NSIG / 8;
		args.ts = (ulong)&ts;
		args.pad = 0;
		ts.tv_nsec = timeout;
		ts.tv_sec = 0;
		arg = &args;
		sz = sizeof(args);
	}else{
		arg = null;
		sz = _NSIG / 8;
	}

	return syscall(__NR_io_uring_enter, ring.ring_fd, submit, wait, flags, arg, sz);
}

int xe_loop::submit(uint wait, ulong timeout){
	uint flags = 0;
	uint submit = ring.sq.sqe_tail - *ring.sq.khead;

	int ret;

	io_uring_smp_store_release(ring.sq.ktail, ring.sq.sqe_tail);

	if(!(ring.flags & IORING_SETUP_IOPOLL)) [[likely]]
		goto enter;
	if(IO_URING_READ_ONCE(*ring.sq.kflags) & IORING_SQ_NEED_WAKEUP) [[unlikely]] {
		flags |= IORING_ENTER_SQ_WAKEUP;

		goto enter;
	}

	if(wait)
		goto enter;
	return submit;
enter:
	if(wait || (ring.flags & IORING_SETUP_IOPOLL))
		flags |= IORING_ENTER_GETEVENTS;
	ret = enter(submit, wait, flags, timeout);

	return ret < 0 ? xe_errno() : ret;
}

int xe_loop::waitsingle(ulong timeout){
	int ret = enter(0, 1, IORING_ENTER_GETEVENTS, timeout);

	if(ret < 0){
		ret = xe_errno();

		return ret == XE_EINTR ? 0 : ret;
	}

	return 0;
}

int xe_loop::run(){
	xe_loop_handle* handle;
	xe_promise* promise;

	ulong packed_handle_index;
	uint handle_index;
	int res;

	uint cqe_head;
	uint cqe_tail;
	uint cqe_mask;
	uint sqe_mask;

	ulong now, timeout;
	xe_rbtree<ulong>::iterator it;
	bool wait;

	cqe_mask = *ring.cq.kring_mask;
	cqe_head = *ring.cq.khead;
	sqe_mask = ring.sq.sqe_head;
	cqe_tail = cqe_head;

	while(true){
		now = xe_time_ns();
		it = timers.begin();
		wait = true;

		if(it != timers.end()){
			/* we have a timer to run */
			timeout = it -> key;

			if(now >= timeout){
				/* just submit, don't wait */
				wait = false;
			}else{
				timeout -= now;
			}
		}else{
			/* wait indefinitely */
			timeout = 0;
		}

		if(num_queued){
			res = submit(wait ? 1 : 0, timeout);

			if(res >= 0){
				xe_log_trace(this, "queued %u handles", res);

				num_handles += res;
				num_queued -= res;
				res = 0;
				cqe_tail = io_uring_smp_load_acquire(ring.cq.ktail);
			}else if(res == XE_EAGAIN || res == XE_EBUSY){
				/* not enough memory for more handles, just wait for returns */
				goto waitsingle;
			}
		}else if(num_handles){
		waitsingle:
			res = 0;
			cqe_tail = io_uring_smp_load_acquire(ring.cq.ktail);

			if(wait && cqe_head == cqe_tail){
				res = waitsingle(timeout);
				cqe_tail = io_uring_smp_load_acquire(ring.cq.ktail);
			}
		}else if(active_timers){
			/* no handles, but we have a timer to run */
			res = wait ? waitsingle(timeout) : 0;
		}else{
			/* nothing else to do */
			break;
		}

		if(res && res != XE_ETIME)
			return res;
		now = xe_time_ns();

		for(it = timers.begin(); it != timers.end(); it = timers.begin()){
			xe_timer& timer = xe_containerof(*it, &xe_timer::expire);

			if(now < timer.expire.key)
				break;
			timers.erase(it);

			run_timer(timer, now);
		}

		xe_log_trace(this, "processing %u handles", cqe_tail - cqe_head);

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
				case XE_LOOP_HANDLE_SOCKET:
					xe_socket::io(*handle, res);

					break;
				case XE_LOOP_HANDLE_FILE:
					xe_file::io(*handle, res);

					break;
				case XE_LOOP_HANDLE_USER:
					if(!handle -> callback)
						break;
					handle -> callback(*this, handle -> user_data, handle -> u1, handle -> u2, res);

					break;
				case XE_LOOP_HANDLE_PROMISE:
					promise = (xe_promise*)handle -> user_data;
					promise -> result_ = res;
					promise -> handle_ = READY_HANDLE;

					if(promise -> waiter)
						promise -> waiter.resume();
					break;
				case XURL_CTX:
					xurl::xurl_ctx::io(*handle, res);

					break;
				default:
					xe_notreached();

					break;
			}
		}

		io_uring_smp_store_release(ring.cq.khead, cqe_tail);
	}

	return 0;
}

xe_cstr xe_loop::class_name(){
	return "xe_loop";
}