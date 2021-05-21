#include "xe/common.h"
#include "async.h"

void xe_promise_resolve(xe_promise* promise, int result){
	promise -> resolve(result);
}

void xe_promise_sethandle(xe_promise* promise, int handle){
	promise -> handle = handle;
}

#define PROMISE(op, ...)																\
	xe_promise promise;																	\
																						\
	int ret = xe_loop_##op(loop, ##__VA_ARGS__, XE_LOOP_PROMISE, &promise, null, 0, 0);	\
																						\
	if(ret < 0)																			\
		xe_promise_resolve(&promise, ret);												\
	else																				\
		xe_promise_sethandle(&promise, ret);											\
	return promise;

xe_promise xe_async_nop(xe_loop* loop){
	PROMISE(nop)
}

xe_promise xe_async_openat(xe_loop* loop, int fd, xe_cstr path, uint flags, mode_t mode){
	PROMISE(openat, fd, path, flags, mode)
}

xe_promise xe_async_openat2(xe_loop* loop, int fd, xe_cstr path, struct open_how* how){
	PROMISE(openat2, fd, path, how)
}

xe_promise xe_async_read(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset){
	PROMISE(read, fd, buf, len, offset)
}

xe_promise xe_async_write(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset){
	PROMISE(write, fd, buf, len, offset)
}

xe_promise xe_async_readv(xe_loop* loop, int fd, iovec* iovecs, uint vlen, ulong offset){
	PROMISE(readv, fd, iovecs, vlen, offset)
}

xe_promise xe_async_writev(xe_loop* loop, int fd, iovec* iovecs, uint vlen, ulong offset){
	PROMISE(writev, fd, iovecs, vlen, offset)
}

xe_promise xe_async_read_fixed(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset, uint buf_index){
	PROMISE(read_fixed, fd, buf, len, offset, buf_index)
}

xe_promise xe_async_write_fixed(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset, uint buf_index){
	PROMISE(write_fixed, fd, buf, len, offset, buf_index)
}

xe_promise xe_async_fsync(xe_loop* loop, int fd, uint flags){
	PROMISE(fsync, fd, flags)
}

xe_promise xe_async_fallocate(xe_loop* loop, int fd, int mode, ulong offset, ulong len){
	PROMISE(fallocate, fd, mode, offset, len)
}

xe_promise xe_async_fadvise(xe_loop* loop, int fd, ulong offset, ulong len, uint advice){
	PROMISE(fadvise, fd, offset, len, advice)
}

xe_promise xe_async_madvise(xe_loop* loop, xe_ptr addr, ulong len, uint advice){
	PROMISE(madvise, addr, len, advice)
}

xe_promise xe_async_sync_file_range(xe_loop* loop, int fd, uint len, ulong offset, uint flags){
	PROMISE(sync_file_range, fd, len, offset, flags)
}

xe_promise xe_async_statx(xe_loop* loop, int fd, xe_cstr path, uint flags, uint mask, struct statx* statx){
	PROMISE(statx, fd, path, flags, mask, statx)
}

xe_promise xe_async_renameat(xe_loop* loop, int old_fd, xe_cstr old_path, int new_fd, xe_cstr new_path, uint flags){
	PROMISE(renameat, old_fd, old_path, new_fd, new_path, flags)
}

xe_promise xe_async_unlinkat(xe_loop* loop, int fd, xe_cstr path, uint flags){
	PROMISE(unlinkat, fd, path, flags)
}

xe_promise xe_async_splice(xe_loop* loop, int fd_in, ulong off_in, int fd_out, ulong off_out, uint len, uint flags){
	PROMISE(splice, fd_in, off_in, fd_out, off_out, len, flags)
}

xe_promise xe_async_tee(xe_loop* loop, int fd_in, int fd_out, uint len, uint flags){
	PROMISE(tee, fd_in, fd_out, len, flags)
}

xe_promise xe_async_connect(xe_loop* loop, int fd, sockaddr* addr, socklen_t addrlen){
	PROMISE(connect, fd, addr, addrlen)
}

xe_promise xe_async_accept(xe_loop* loop, int fd, sockaddr* addr, socklen_t* addrlen, uint flags){
	PROMISE(accept, fd, addr, addrlen, flags)
}

xe_promise xe_async_recv(xe_loop* loop, int fd, xe_buf buf, uint len, uint flags){
	PROMISE(recv, fd, buf, len, flags)
}

xe_promise xe_async_send(xe_loop* loop, int fd, xe_buf buf, uint len, uint flags){
	PROMISE(send, fd, buf, len, flags)
}

xe_promise xe_async_recvmsg(xe_loop* loop, int fd, struct msghdr* msg, uint flags){
	PROMISE(recvmsg, fd, msg, flags)
}

xe_promise xe_async_sendmsg(xe_loop* loop, int fd, struct msghdr* msg, uint flags){
	PROMISE(sendmsg, fd, msg, flags)
}

xe_promise xe_async_poll(xe_loop* loop, int fd, uint mask){
	PROMISE(poll, fd, mask)
}

xe_promise xe_async_poll_cancel(xe_loop* loop, int handle){
	PROMISE(poll_cancel, handle)
}

xe_promise xe_async_cancel(xe_loop* loop, int handle, uint flags){
	PROMISE(cancel, handle, flags)
}

xe_promise xe_async_files_update(xe_loop* loop, int* fds, uint len, uint offset){
	PROMISE(files_update, fds, len, offset)
}

xe_promise xe_async::nop(){
	return xe_async_nop(loop);
}

xe_promise xe_async::openat(int fd, xe_cstr path, uint flags, mode_t mode){
	return xe_async_openat(loop, fd, path, flags, mode);
}

xe_promise xe_async::openat2(int fd, xe_cstr path, struct open_how* how){
	return xe_async_openat2(loop, fd, path, how);
}

xe_promise xe_async::read(int fd, xe_buf buf, uint len, ulong offset){
	return xe_async_read(loop, fd, buf, len, offset);
}

xe_promise xe_async::write(int fd, xe_buf buf, uint len, ulong offset){
	return xe_async_write(loop, fd, buf, len, offset);
}

xe_promise xe_async::readv(int fd, iovec* iovecs, uint vlen, ulong offset){
	return xe_async_readv(loop, fd, iovecs, vlen, offset);
}

xe_promise xe_async::writev(int fd, iovec* iovecs, uint vlen, ulong offset){
	return xe_async_writev(loop, fd, iovecs, vlen, offset);
}

xe_promise xe_async::read_fixed(int fd, xe_buf buf, uint len, ulong offset, uint buf_index){
	return xe_async_read_fixed(loop, fd, buf, len, offset, buf_index);
}

xe_promise xe_async::write_fixed(int fd, xe_buf buf, uint len, ulong offset, uint buf_index){
	return xe_async_write_fixed(loop, fd, buf, len, offset, buf_index);
}

xe_promise xe_async::fsync(int fd, uint flags){
	return xe_async_fsync(loop, fd, flags);
}

xe_promise xe_async::fallocate(int fd, int mode, ulong offset, ulong len){
	return xe_async_fallocate(loop, fd, mode, offset, len);
}

xe_promise xe_async::fadvise(int fd, ulong offset, ulong len, uint advice){
	return xe_async_fadvise(loop, fd, offset, len, advice);
}

xe_promise xe_async::madvise(xe_ptr addr, ulong len, uint advice){
	return xe_async_madvise(loop, addr, len, advice);
}

xe_promise xe_async::sync_file_range(int fd, uint len, ulong offset, uint flags){
	return xe_async_sync_file_range(loop, fd, len, offset, flags);
}

xe_promise xe_async::statx(int fd, xe_cstr path, uint flags, uint mask, struct statx* statx){
	return xe_async_statx(loop, fd, path, flags, mask, statx);
}

xe_promise xe_async::renameat(int old_fd, xe_cstr old_path, int new_fd, xe_cstr new_path, uint flags){
	return xe_async_renameat(loop, old_fd, old_path, new_fd, new_path, flags);
}

xe_promise xe_async::unlinkat(int fd, xe_cstr path, uint flags){
	return xe_async_unlinkat(loop, fd, path, flags);
}

xe_promise xe_async::splice(int fd_in, ulong off_in, int fd_out, ulong off_out, uint len, uint flags){
	return xe_async_splice(loop, fd_in, off_in, fd_out, off_out, len, flags);
}

xe_promise xe_async::tee(int fd_in, int fd_out, uint len, uint flags){
	return xe_async_tee(loop, fd_in, fd_out, len, flags);
}

xe_promise xe_async::connect(int fd, sockaddr* addr, socklen_t addrlen){
	return xe_async_connect(loop, fd, addr, addrlen);
}

xe_promise xe_async::accept(int fd, sockaddr* addr, socklen_t* addrlen, uint flags){
	return xe_async_accept(loop, fd, addr, addrlen, flags);
}

xe_promise xe_async::recv(int fd, xe_buf buf, uint len, uint flags){
	return xe_async_recv(loop, fd, buf, len, flags);
}

xe_promise xe_async::send(int fd, xe_buf buf, uint len, uint flags){
	return xe_async_send(loop, fd, buf, len, flags);
}

xe_promise xe_async::recvmsg(int fd, struct msghdr* msg, uint flags){
	return xe_async_recvmsg(loop, fd, msg, flags);
}

xe_promise xe_async::sendmsg(int fd, struct msghdr* msg, uint flags){
	return xe_async_sendmsg(loop, fd, msg, flags);
}

xe_promise xe_async::poll(int fd, uint mask){
	return xe_async_poll(loop, fd, mask);
}

xe_promise xe_async::poll_cancel(int handle){
	return xe_async_poll_cancel(loop, handle);
}

xe_promise xe_async::cancel(int handle, uint flags){
	return xe_async_cancel(loop, handle, flags);
}

xe_promise xe_async::files_update(int* fds, uint len, uint offset){
	return xe_async_files_update(loop, fds, len, offset);
}

int xe_async::run(){
	return xe_loop_run(loop);
}

uint xe_async::remain(){
	return xe_loop_remain(loop);
}

void xe_async::destroy(){
	xe_loop_destroy(loop);
}