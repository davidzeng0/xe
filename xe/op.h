#pragma once
#include <liburing.h>
#include "xstd/types.h"
#include "xe.h"

class xe_op{
	io_uring_sqe sqe;

	friend class xe_loop;
public:
	xe_op() = default;

	static xe_op nop();

	static xe_op openat				(int dfd, xe_cstr path, uint flags, mode_t mode, uint file_index = 0);
	static xe_op openat2			(int dfd, xe_cstr path, open_how* how, uint file_index = 0);

	static xe_op close				(int fd);
	static xe_op close_direct		(uint file_index);

	static xe_op read				(int fd, xe_ptr buf, uint len, long offset, uint flags = 0);
	static xe_op write				(int fd, xe_cptr buf, uint len, long offset, uint flags = 0);
	static xe_op readv				(int fd, const iovec* iovecs, uint vlen, long offset, uint flags = 0);
	static xe_op writev				(int fd, const iovec* iovecs, uint vlen, long offset, uint flags = 0);
	static xe_op read_fixed			(int fd, xe_ptr buf, uint len, long offset, uint buf_index, uint flags = 0);
	static xe_op write_fixed		(int fd, xe_cptr buf, uint len, long offset, uint buf_index, uint flags = 0);

	static xe_op fsync				(int fd, uint flags);
	static xe_op sync_file_range	(int fd, uint len, long offset, uint flags);
	static xe_op fallocate			(int fd, int mode, long offset, long len);

	static xe_op fadvise			(int fd, ulong offset, uint len, uint advice);
	static xe_op madvise			(xe_ptr addr, uint len, uint advice);

	static xe_op renameat			(int old_dfd, xe_cstr old_path, int new_dfd, xe_cstr new_path, uint flags);
	static xe_op unlinkat			(int dfd, xe_cstr path, uint flags);
	static xe_op mkdirat			(int dfd, xe_cstr path, mode_t mode);
	static xe_op symlinkat			(xe_cstr target, int newdirfd, xe_cstr linkpath);
	static xe_op linkat				(int old_dfd, xe_cstr old_path, int new_dfd, xe_cstr new_path, uint flags);

	static xe_op fgetxattr			(int fd, xe_cstr name, char* value, uint len);
	static xe_op fsetxattr			(int fd, xe_cstr name, xe_cstr value, uint len, uint flags);
	static xe_op getxattr			(xe_cstr path, xe_cstr name, char* value, uint len);
	static xe_op setxattr			(xe_cstr path, xe_cstr name, xe_cstr value, uint len, uint flags);

	static xe_op splice				(int fd_in, long off_in, int fd_out, long off_out, uint len, uint flags);
	static xe_op tee				(int fd_in, int fd_out, uint len, uint flags);

	static xe_op statx				(int fd, xe_cstr path, uint flags, uint mask, struct statx* statx);

	static xe_op socket				(int af, int type, int protocol, uint flags, uint file_index = 0);

	static xe_op connect			(int fd, const sockaddr* addr, socklen_t addrlen);
	static xe_op accept				(int fd, sockaddr* addr, socklen_t* addrlen, uint flags, uint file_index = 0);

	static xe_op recv				(int fd, xe_ptr buf, uint len, uint flags);
	static xe_op send				(int fd, xe_cptr buf, uint len, uint flags);
	static xe_op recvmsg			(int fd, msghdr* msg, uint flags);
	static xe_op sendmsg			(int fd, const msghdr* msg, uint flags);
	static xe_op send_zc			(int fd, xe_cptr buf, uint len, uint flags, uint buf_index = 0);
	static xe_op sendto_zc			(int fd, xe_cptr buf, uint len, uint flags, const sockaddr* addr, socklen_t addrlen, uint buf_index = 0);

	static xe_op shutdown			(int fd, int how);

	static xe_op poll				(int fd, uint mask);
	static xe_op poll_update		(uint mask, uint flags);
	static xe_op epoll_ctl			(int epfd, int op, int fd, epoll_event* events);

	static xe_op poll_cancel		();
	static xe_op cancel				(uint flags);
	static xe_op cancel				(int fd, uint flags);
	static xe_op cancel_fixed		(uint file_index, uint flags);
	static xe_op cancel_all			();

	static xe_op files_update		(int* fds, uint len, uint offset);

	static xe_op provide_buffers	(xe_ptr addr, uint len, ushort nr, ushort bgid, ushort bid);
	static xe_op remove_buffers		(ushort nr, ushort bgid);

	xe_op& flags(byte flags){
		sqe.flags = flags;

		return *this;
	}

	xe_op& fixed(){
		return flags(IOSQE_FIXED_FILE);
	}

	xe_op& drain(){
		return flags(IOSQE_IO_DRAIN);
	}

	xe_op& link(){
		return flags(IOSQE_IO_LINK);
	}

	xe_op& hardlink(){
		return flags(IOSQE_IO_HARDLINK);
	}

	xe_op& async(){
		return flags(IOSQE_ASYNC);
	}

	xe_op& buffer_select(uint bgid){
		flags(IOSQE_BUFFER_SELECT);

		sqe.buf_group = bgid;

		return *this;
	}

	xe_op& skip_success() = delete; /* not supported */

	xe_op& ioprio(ushort ioprio){
		sqe.ioprio = ioprio;

		return *this;
	}

	xe_op& ioprio_orflags(ushort flags){
		sqe.ioprio |= flags;

		return *this;
	}

	xe_op& recvsend_poll_first(){
		return ioprio_orflags(IORING_RECVSEND_POLL_FIRST);
	}

	xe_op& recv_multishot(){
		return ioprio_orflags(IORING_RECV_MULTISHOT);
	}

	xe_op& recvsend_fixed_buf(){
		return ioprio_orflags(IORING_RECVSEND_FIXED_BUF);
	}

	xe_op& accept_multishot(){
		return ioprio_orflags(IORING_ACCEPT_MULTISHOT);
	}

	xe_op& poll_flags(uint flags){
		sqe.len |= flags;

		return *this;
	}

	xe_op& poll_add_multi(){
		return poll_flags(IORING_POLL_ADD_MULTI);
	}

	xe_op& poll_update_events(){
		return poll_flags(IORING_POLL_UPDATE_EVENTS);
	}

	xe_op& poll_update_user_data(xe_req& new_req){
		sqe.off = (ulong)&new_req;

		return poll_flags(IORING_POLL_UPDATE_USER_DATA);
	}

	xe_op& poll_add_level(){
		return poll_flags(IORING_POLL_ADD_LEVEL);
	}

	~xe_op() = default;
};