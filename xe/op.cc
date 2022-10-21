#include "xutil/endian.h"
#include "xutil/mem.h"
#include "xconfig/config.h"
#include "op.h"

static inline void xe_sqe_init(io_uring_sqe& sqe, io_uring_op op){
	sqe.opcode = op;
	sqe.flags = 0;
	sqe.ioprio = 0;
	sqe.personality = 0;
}

static inline void xe_sqe_rw(io_uring_sqe& sqe, int fd, xe_cptr addr, uint len, ulong offset, uint rw_flags){
	sqe.fd = fd;
	sqe.addr = (ulong)addr;
	sqe.len = len;
	sqe.off = offset;
	sqe.rw_flags = rw_flags;
}

static inline void xe_sqe_rw_fixed(io_uring_sqe& sqe, int fd, xe_cptr addr, uint len, ulong offset, uint rw_flags, uint buf_index){
	xe_sqe_rw(sqe, fd, addr, len, offset, rw_flags);

	sqe.buf_index = buf_index;
}

static inline void xe_sqe_close(io_uring_sqe& sqe, int fd, uint file_index){
	xe_sqe_rw_fixed(sqe, fd, null, 0, 0, 0, 0);

	sqe.file_index = file_index;
}

static inline void xe_sqe_sync(io_uring_sqe& sqe, int fd, uint len, ulong offset, uint sync_flags){
	xe_sqe_rw_fixed(sqe, fd, null, len, offset, sync_flags, 0);

	sqe.splice_fd_in = 0;
}

static inline void xe_sqe_advise(io_uring_sqe& sqe, xe_cptr addr, uint len, ulong offset, uint advice){
	sqe.addr = (ulong)addr;
	sqe.len = len;
	sqe.off = offset;
	sqe.fadvise_advice = advice;
	sqe.buf_index = 0;
	sqe.splice_fd_in = 0;
}

static inline void xe_sqe_fs(io_uring_sqe& sqe, int fd0, xe_cptr ptr0, int fd1, xe_cptr ptr1, uint fs_flags){
	sqe.fd = fd0;
	sqe.addr = (ulong)ptr0;
	sqe.len = fd1;
	sqe.off = (ulong)ptr1;
	sqe.rw_flags = fs_flags;
	sqe.buf_index = 0;
	sqe.splice_fd_in = 0;
}

static inline void xe_sqe_fxattr(io_uring_sqe& sqe, int fd, xe_cptr name, xe_cptr value, uint len, uint xattr_flags){
	sqe.fd = fd;
	sqe.addr = (ulong)name;
	sqe.len = len;
	sqe.addr2 = (ulong)value;
	sqe.xattr_flags = xattr_flags;
}

static inline void xe_sqe_xattr(io_uring_sqe& sqe, xe_cstr path, xe_cptr name, xe_cptr value, uint len, uint xattr_flags){
	sqe.addr3 = (ulong)path;
	sqe.addr = (ulong)name;
	sqe.len = len;
	sqe.addr2 = (ulong)value;
	sqe.xattr_flags = xattr_flags;
}

static inline void xe_sqe_splice(io_uring_sqe& sqe, int fd_in, ulong off_in, int fd_out, ulong off_out, uint len, uint splice_flags){
	sqe.splice_fd_in = fd_in;
	sqe.splice_off_in = off_in;
	sqe.fd = fd_out;
	sqe.off = off_out;
	sqe.len = len;
	sqe.splice_flags = splice_flags;
}

static inline void xe_sqe_socket(io_uring_sqe& sqe, int fd, xe_cptr addr, uint len, ulong offset, uint socket_flags, uint file_index){
	xe_sqe_rw_fixed(sqe, fd, addr, len, offset, socket_flags, 0);

	sqe.file_index = file_index;
}

static inline void xe_sqe_socket_rw(io_uring_sqe& sqe, int fd, xe_cptr addr, uint len, uint msg_flags){
	xe_sqe_rw(sqe, fd, addr, len, 0, msg_flags);

	sqe.file_index = 0;
}

static inline void xe_sqe_buffer(io_uring_sqe& sqe, xe_ptr addr, uint len, ushort nr, ushort bgid, ushort bid){
	sqe.fd = nr;
	sqe.addr = (ulong)addr;
	sqe.len = len;
	sqe.off = bid;
	sqe.buf_group = bgid;
	sqe.rw_flags = 0;
}

static inline uint xe_poll_mask(uint mask){
	if constexpr(XE_BYTE_ORDER == XE_BIG_ENDIAN)
		mask = __swahw32(mask);
	return mask;
}

xe_op xe_op::nop(){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_NOP);

	return op;
}

xe_op xe_op::openat(int dfd, xe_cstr path, uint flags, mode_t mode, uint file_index){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_OPENAT);

	op.sqe.fd = dfd;
	op.sqe.addr = (ulong)path;
	op.sqe.len = mode;
	op.sqe.open_flags = flags;
	op.sqe.buf_index = 0;
	op.sqe.file_index = file_index;

	return op;
}

xe_op xe_op::openat2(int dfd, xe_cstr path, open_how* how, uint file_index){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_OPENAT2);

	op.sqe.fd = dfd;
	op.sqe.addr = (ulong)path;
	op.sqe.len = sizeof(*how);
	op.sqe.off = (ulong)how;
	op.sqe.buf_index = 0;
	op.sqe.file_index = file_index;

	return op;
}

xe_op xe_op::close(int fd){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_CLOSE);
	xe_sqe_close(op.sqe, fd, 0);

	return op;
}

xe_op xe_op::close_direct(uint file_index){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_CLOSE);
	xe_sqe_close(op.sqe, 0, file_index);

	return op;
}

xe_op xe_op::read(int fd, xe_ptr buf, uint len, long offset, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_READ);
	xe_sqe_rw(op.sqe, fd, buf, len, offset, flags);

	return op;
}

xe_op xe_op::write(int fd, xe_cptr buf, uint len, long offset, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_WRITE);
	xe_sqe_rw(op.sqe, fd, buf, len, offset, flags);

	return op;
}

xe_op xe_op::readv(int fd, const iovec* iovecs, uint vlen, long offset, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_READV);
	xe_sqe_rw(op.sqe, fd, iovecs, vlen, offset, flags);

	return op;
}

xe_op xe_op::writev(int fd, const iovec* iovecs, uint vlen, long offset, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_WRITEV);
	xe_sqe_rw(op.sqe, fd, iovecs, vlen, offset, flags);

	return op;
}

xe_op xe_op::read_fixed(int fd, xe_ptr buf, uint len, long offset, uint buf_index, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_READ_FIXED);
	xe_sqe_rw_fixed(op.sqe, fd, buf, len, offset, flags, buf_index);

	return op;
}

xe_op xe_op::write_fixed(int fd, xe_cptr buf, uint len, long offset, uint buf_index, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_WRITE_FIXED);
	xe_sqe_rw_fixed(op.sqe, fd, buf, len, offset, flags, buf_index);

	return op;
}

xe_op xe_op::fsync(int fd, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_FSYNC);
	xe_sqe_sync(op.sqe, fd, 0, 0, flags);

	return op;
}

xe_op xe_op::sync_file_range(int fd, uint len, long offset, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_SYNC_FILE_RANGE);
	xe_sqe_sync(op.sqe, fd, len, offset, flags);

	return op;
}

xe_op xe_op::fallocate(int fd, int mode, long offset, long len){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_FALLOCATE);
	xe_sqe_rw_fixed(op.sqe, fd, (xe_ptr)len, mode, offset, 0, 0);

	op.sqe.splice_fd_in = 0;

	return op;
}

xe_op xe_op::fadvise(int fd, ulong offset, uint len, uint advice){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_FADVISE);
	xe_sqe_advise(op.sqe, null, len, offset, advice);

	op.sqe.fd = fd;

	return op;
}

xe_op xe_op::madvise(xe_ptr addr, uint len, uint advice){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_MADVISE);
	xe_sqe_advise(op.sqe, addr, len, 0, advice);

	return op;
}

xe_op xe_op::renameat(int old_dfd, xe_cstr old_path, int new_dfd, xe_cstr new_path, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_RENAMEAT);
	xe_sqe_fs(op.sqe, old_dfd, old_path, new_dfd, new_path, flags);

	return op;
}

xe_op xe_op::unlinkat(int dfd, xe_cstr path, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_UNLINKAT);
	xe_sqe_fs(op.sqe, dfd, path, 0, null, flags);

	return op;
}

xe_op xe_op::mkdirat(int dfd, xe_cstr path, mode_t mode){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_MKDIRAT);
	xe_sqe_fs(op.sqe, dfd, path, mode, null, 0);

	return op;
}

xe_op xe_op::symlinkat(xe_cstr target, int newdirfd, xe_cstr linkpath){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_SYMLINKAT);
	xe_sqe_fs(op.sqe, newdirfd, target, 0, linkpath, 0);

	return op;
}

xe_op xe_op::linkat(int old_dfd, xe_cstr old_path, int new_dfd, xe_cstr new_path, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_LINKAT);
	xe_sqe_fs(op.sqe, old_dfd, old_path, new_dfd, new_path, flags);

	return op;
}

xe_op xe_op::fgetxattr(int fd, xe_cstr name, char* value, uint len){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_FGETXATTR);
	xe_sqe_fxattr(op.sqe, fd, name, value, len, 0);

	return op;
}

xe_op xe_op::fsetxattr(int fd, xe_cstr name, xe_cstr value, uint len, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_FSETXATTR);
	xe_sqe_fxattr(op.sqe, fd, name, value, len, flags);

	return op;
}

xe_op xe_op::getxattr(xe_cstr path, xe_cstr name, char* value, uint len){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_GETXATTR);
	xe_sqe_xattr(op.sqe, path, name, value, len, 0);

	return op;
}

xe_op xe_op::setxattr(xe_cstr path, xe_cstr name, xe_cstr value, uint len, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_SETXATTR);
	xe_sqe_xattr(op.sqe, path, name, value, len, flags);

	return op;
}

xe_op xe_op::splice(int fd_in, long off_in, int fd_out, long off_out, uint len, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_SPLICE);
	xe_sqe_splice(op.sqe, fd_in, off_in, fd_out, off_out, len, flags);

	return op;
}

xe_op xe_op::tee(int fd_in, int fd_out, uint len, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_TEE);
	xe_sqe_splice(op.sqe, fd_in, 0, fd_out, 0, len, flags);

	return op;
}

xe_op xe_op::statx(int fd, xe_cstr path, uint flags, uint mask, struct statx* statx){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_STATX);
	xe_sqe_rw_fixed(op.sqe, fd, path, mask, (ulong)statx, flags, 0);

	return op;
}

xe_op xe_op::socket(int af, int type, int protocol, uint flags, uint file_index){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_SOCKET);
	xe_sqe_socket(op.sqe, af, null, protocol, type, flags, file_index);

	return op;
}

xe_op xe_op::connect(int fd, const sockaddr* addr, socklen_t addrlen){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_CONNECT);
	xe_sqe_socket(op.sqe, fd, addr, 0, addrlen, 0, 0);

	return op;
}

xe_op xe_op::accept(int fd, sockaddr* addr, socklen_t* addrlen, uint flags, uint file_index){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_ACCEPT);
	xe_sqe_socket(op.sqe, fd, addr, 0, (ulong)addrlen, flags, file_index);

	return op;
}

xe_op xe_op::recv(int fd, xe_ptr buf, uint len, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_RECV);
	xe_sqe_socket_rw(op.sqe, fd, buf, len, flags);

	return op;
}

xe_op xe_op::send(int fd, xe_cptr buf, uint len, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_SEND);
	xe_sqe_socket_rw(op.sqe, fd, buf, len, flags);

	return op;
}

xe_op xe_op::recvmsg(int fd, msghdr* msg, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_RECVMSG);
	xe_sqe_socket_rw(op.sqe, fd, msg, 1, flags);

	return op;
}

xe_op xe_op::sendmsg(int fd, const msghdr* msg, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_SENDMSG);
	xe_sqe_socket_rw(op.sqe, fd, msg, 1, flags);

	return op;
}

xe_op xe_op::send_zc(int fd, xe_cptr buf, uint len, uint flags, uint buf_index){
	return sendto_zc(fd, buf, len, flags, null, 0, buf_index);
}

xe_op xe_op::sendto_zc(int fd, xe_cptr buf, uint len, uint flags, const sockaddr* addr, socklen_t addrlen, uint buf_index){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_SEND_ZC);

	op.sqe.fd = fd;
	op.sqe.addr = (ulong)buf;
	op.sqe.len = len;
	op.sqe.addr2 = (ulong)addr;
	op.sqe.addr_len = addrlen;
	op.sqe.msg_flags = flags;
	op.sqe.addr3 = 0;
	op.sqe.buf_index = buf_index;
	op.sqe.__pad2[0] = 0;
	op.sqe.__pad3[0] = 0;

	return op;
}

xe_op xe_op::shutdown(int fd, int how){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_SHUTDOWN);
	xe_sqe_socket(op.sqe, fd, null, how, 0, 0, 0);

	return op;
}

xe_op xe_op::poll(int fd, uint mask){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_POLL_ADD);
	xe_sqe_rw_fixed(op.sqe, fd, null, 0, 0, xe_poll_mask(mask), 0);

	return op;
}

xe_op xe_op::poll_update(uint mask, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_POLL_REMOVE);

	op.sqe.len = flags;
	op.sqe.off = 0;
	op.sqe.poll32_events = xe_poll_mask(mask);
	op.sqe.buf_index = 0;
	op.sqe.splice_fd_in = 0;

	return op;
}

xe_op xe_op::epoll_ctl(int epfd, int epop, int fd, epoll_event* events){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_EPOLL_CTL);

	op.sqe.buf_index = 0;
	op.sqe.splice_fd_in = 0;
	op.sqe.fd = epfd;
	op.sqe.addr = (ulong)events;
	op.sqe.len = epop;
	op.sqe.off = fd;

	return op;
}

xe_op xe_op::poll_cancel(){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_POLL_REMOVE);

	op.sqe.len = 0;
	op.sqe.off = 0;
	op.sqe.poll32_events = 0;
	op.sqe.buf_index = 0;
	op.sqe.splice_fd_in = 0;

	return op;
}

xe_op xe_op::cancel(uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_ASYNC_CANCEL);

	op.sqe.len = 0;
	op.sqe.off = 0;
	op.sqe.cancel_flags = flags;
	op.sqe.splice_fd_in = 0;

	return op;
}

xe_op xe_op::cancel(int fd, uint flags){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_ASYNC_CANCEL);
	xe_sqe_rw(op.sqe, fd, null, 0, 0, flags | IORING_ASYNC_CANCEL_FD);

	op.sqe.splice_fd_in = 0;

	return op;
}

xe_op xe_op::cancel_fixed(uint file_index, uint flags){
	/* safe to cast file_index to int here */
	return cancel(file_index, flags | IORING_ASYNC_CANCEL_FD_FIXED);
}

xe_op xe_op::cancel_all(){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_ASYNC_CANCEL);
	xe_sqe_rw(op.sqe, 0, null, 0, 0, IORING_ASYNC_CANCEL_ANY);

	op.sqe.splice_fd_in = 0;

	return op;
}

xe_op xe_op::files_update(int* fds, uint len, uint offset){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_FILES_UPDATE);

	op.sqe.addr = (ulong)fds;
	op.sqe.len = len;
	op.sqe.off = offset;
	op.sqe.rw_flags = 0;
	op.sqe.splice_fd_in = 0;

	return op;
}

xe_op xe_op::provide_buffers(xe_ptr addr, uint len, ushort nr, ushort bgid, ushort bid){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_PROVIDE_BUFFERS);
	xe_sqe_buffer(op.sqe, addr, len, nr, bgid, bid);

	return op;
}

xe_op xe_op::remove_buffers(ushort nr, ushort bgid){
	xe_op op;

	xe_sqe_init(op.sqe, IORING_OP_REMOVE_BUFFERS);
	xe_sqe_buffer(op.sqe, null, 0, nr, bgid, 0);

	return op;
}
