#pragma once
#ifdef XE_COROUTINES_EXPERIMENTAL
#include <experimental/coroutine>

namespace std{
	using std::experimental::coroutine_handle;
	using std::experimental::suspend_never;
	using std::experimental::suspend_always;
};
#else
#include <coroutine>
#endif
#include "loop.h"

class xe_promise{
	std::coroutine_handle<> waiter;

	int result;
	int ready;
	int pad;
	int handle;

	void resolve(int res){
		result = res;
		ready = true;
		handle = -1;

		if(waiter)
			waiter.resume();
	}

	friend void xe_promise_resolve(xe_promise*, int);
	friend void xe_promise_sethandle(xe_promise*, int);

	public:
		xe_promise(){
			ready = false;
			waiter = nullptr;
			handle = -1;
		}

		bool await_ready(){
			return ready;
		}

		void await_suspend(std::coroutine_handle<> handle){
			waiter = handle;
		}

		int await_resume(){
			return result;
		}

		int get_handle(){
			return handle;
		}
};

void xe_promise_resolve(xe_promise*, int);

xe_promise xe_async_nop				(xe_loop* loop);

xe_promise xe_async_openat			(xe_loop* loop, int fd, xe_cstr path, uint flags, mode_t mode);
xe_promise xe_async_openat2			(xe_loop* loop, int fd, xe_cstr path, struct open_how* how);

xe_promise xe_async_read			(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset);
xe_promise xe_async_write			(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset);
xe_promise xe_async_readv			(xe_loop* loop, int fd, iovec* iovecs, uint vlen, ulong offset);
xe_promise xe_async_writev			(xe_loop* loop, int fd, iovec* iovecs, uint vlen, ulong offset);
xe_promise xe_async_read_fixed		(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset, uint buf_index);
xe_promise xe_async_write_fixed		(xe_loop* loop, int fd, xe_buf buf, uint len, ulong offset, uint buf_index);

xe_promise xe_async_fsync			(xe_loop* loop, int fd, uint flags);
xe_promise xe_async_fallocate		(xe_loop* loop, int fd, int mode, ulong offset, ulong len);
xe_promise xe_async_fadvise			(xe_loop* loop, int fd, ulong offset, ulong len, uint advice);

xe_promise xe_async_madvise			(xe_loop* loop, xe_ptr addr, ulong len, uint advice);

xe_promise xe_async_sync_file_range	(xe_loop* loop, int fd, uint len, ulong offset, uint flags);

xe_promise xe_async_statx			(xe_loop* loop, int fd, xe_cstr path, uint flags, uint mask, struct statx* statx);

xe_promise xe_async_renameat		(xe_loop* loop, int old_fd, xe_cstr old_path, int new_fd, xe_cstr new_path, uint flags);
xe_promise xe_async_unlinkat		(xe_loop* loop, int fd, xe_cstr path, uint flags);

xe_promise xe_async_splice			(xe_loop* loop, int fd_in, ulong off_in, int fd_out, ulong off_out, uint len, uint flags);
xe_promise xe_async_tee				(xe_loop* loop, int fd_in, int fd_out, uint len, uint flags);

xe_promise xe_async_connect			(xe_loop* loop, int fd, sockaddr* addr, socklen_t addrlen);
xe_promise xe_async_accept			(xe_loop* loop, int fd, sockaddr* addr, socklen_t* addrlen, uint flags);
xe_promise xe_async_recv			(xe_loop* loop, int fd, xe_buf buf, uint len, uint flags);
xe_promise xe_async_send			(xe_loop* loop, int fd, xe_buf buf, uint len, uint flags);

xe_promise xe_async_recvmsg			(xe_loop* loop, int fd, struct msghdr* msg, uint flags);
xe_promise xe_async_sendmsg			(xe_loop* loop, int fd, struct msghdr* msg, uint flags);

xe_promise xe_async_poll			(xe_loop* loop, int fd, uint mask);
xe_promise xe_async_poll_cancel		(xe_loop* loop, int handle);

xe_promise xe_async_cancel			(xe_loop* loop, int handle, uint flags);

xe_promise xe_async_files_update	(xe_loop* loop, int* fds, uint len, uint offset);

class xe_async{
	xe_loop* loop;

	public:
		xe_async(xe_loop* ploop){
			loop = ploop;
		}

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

		int run();

		uint remain();

		void destroy();
};