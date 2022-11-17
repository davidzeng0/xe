#include <unistd.h>
#include <sys/syscall.h>
#include "file.h"
#include "../error.h"

void xe_open_req::complete(xe_req& req, int res, uint flags){
	xe_open_req& open_req = (xe_open_req&)req;

	open_req.file -> open(res);

	if(res > 0)
		res = 0;
	if(open_req.callback) open_req.callback(open_req, res);
}

void xe_open_promise::complete(xe_req& req, int res, uint flags){
	xe_open_promise& promise = (xe_open_promise&)req;

	promise.file -> open(res);

	if(res > 0)
		res = 0;
	xe_promise::complete(req, res, flags);
}

xe_open_promise::xe_open_promise(){
	event = complete;
}

void xe_file::open(int res){
	opening = false;

	if(res >= 0) fd_ = res;
}

int xe_file::open(xe_open_req& req, xe_op op){
	if(opening)
		return XE_EALREADY;
	if(fd_ >= 0)
		return XE_STATE;
	xe_return_error(loop_ -> run(req, op));

	req.file = this;
	opening = true;

	return 0;
}

xe_open_promise xe_file::open(xe_op op){
	xe_open_promise promise;
	int res;

	if(opening)
		res = XE_EALREADY;
	else if(fd_ >= 0)
		res = XE_STATE;
	else
		res = loop_ -> run(promise, op);
	if(res){
		promise.result_ = res;
		promise.ready_ = true;
	}else{
		promise.file = this;
		opening = true;
	}

	return promise;
}

int xe_file::open_sync(xe_cstr path, uint flags){
	return openat_sync(AT_FDCWD, path, flags);
}

int xe_file::openat_sync(int dfd, xe_cstr path, uint flags){
	int fd = ::openat(dfd, path, flags);

	if(fd < 0)
		return xe_errno();
	fd_ = fd;

	return 0;
}

int xe_file::open(xe_open_req& req, xe_cstr path, uint flags){
	return openat(req, AT_FDCWD, path, flags);
}

int xe_file::openat(xe_open_req& req, int dfd, xe_cstr path, uint flags){
	return open(req, xe_op::openat(dfd, path, flags, 0));
}

xe_open_promise xe_file::open(xe_cstr path, uint flags){
	return openat(AT_FDCWD, path, flags);
}

xe_open_promise xe_file::openat(int dfd, xe_cstr path, uint flags){
	return open(xe_op::openat(dfd, path, flags, 0));
}

int xe_file::open2_sync(xe_cstr path, open_how* how){
	return openat2_sync(AT_FDCWD, path, how);
}

int xe_file::openat2_sync(int dfd, xe_cstr path, open_how* how){
	int fd = syscall(__NR_openat2, dfd, path, how);

	if(fd < 0)
		return xe_errno();
	fd_ = fd;

	return 0;
}

int xe_file::open2(xe_open_req& req, xe_cstr path, open_how* how){
	return openat2(req, AT_FDCWD, path, how);
}

int xe_file::openat2(xe_open_req& req, int dfd, xe_cstr path, open_how* how){
	return open(req, xe_op::openat2(dfd, path, how));
}

xe_open_promise xe_file::open2(xe_cstr path, open_how* how){
	return openat2(AT_FDCWD, path, how);
}

xe_open_promise xe_file::openat2(int dfd, xe_cstr path, open_how* how){
	return open(xe_op::openat2(dfd, path, how));
}

int xe_file::read_sync(xe_ptr buf, uint len, long offset){
	if(fd_ < 0)
		return XE_STATE;
	int read = pread(fd_, buf, len, offset);

	return read < 0 ? xe_errno() : read;
}

int xe_file::write_sync(xe_cptr buf, uint len, long offset){
	if(fd_ < 0)
		return XE_STATE;
	int wrote = pwrite(fd_, buf, len, offset);

	return wrote < 0 ? xe_errno() : wrote;
}

int xe_file::read(xe_req& req, xe_ptr buf, uint len, long offset){
	if(fd_ < 0)
		return XE_STATE;
	return loop_ -> run(req, xe_op::read(fd_, buf, len, offset));
}

int xe_file::write(xe_req& req, xe_cptr buf, uint len, long offset){
	if(fd_ < 0)
		return XE_STATE;
	return loop_ -> run(req, xe_op::write(fd_, buf, len, offset));
}

xe_promise xe_file::read(xe_ptr buf, uint len, long offset){
	if(fd_ < 0)
		return xe_promise::done(XE_STATE);
	return loop_ -> run(xe_op::read(fd_, buf, len, offset));
}

xe_promise xe_file::write(xe_cptr buf, uint len, long offset){
	if(fd_ < 0)
		return xe_promise::done(XE_STATE);
	return loop_ -> run(xe_op::write(fd_, buf, len, offset));
}

int xe_file::readv_sync(const iovec* iovecs, uint vlen, long offset){
	if(fd_ < 0)
		return XE_STATE;
	int read = preadv(fd_, iovecs, vlen, offset);

	return read < 0 ? xe_errno() : read;
}

int xe_file::writev_sync(const iovec* iovecs, uint vlen, long offset){
	if(fd_ < 0)
		return XE_STATE;
	int wrote = pwritev(fd_, iovecs, vlen, offset);

	return wrote < 0 ? xe_errno() : wrote;
}

int xe_file::readv(xe_req& req, const iovec* iovecs, uint vlen, long offset){
	if(fd_ < 0)
		return XE_STATE;
	return loop_ -> run(req, xe_op::readv(fd_, iovecs, vlen, offset));
}

int xe_file::writev(xe_req& req, const iovec* iovecs, uint vlen, long offset){
	if(fd_ < 0)
		return XE_STATE;
	return loop_ -> run(req, xe_op::writev(fd_, iovecs, vlen, offset));
}

xe_promise xe_file::readv(const iovec* iovecs, uint vlen, long offset){
	if(fd_ < 0)
		return xe_promise::done(XE_STATE);
	return loop_ -> run(xe_op::readv(fd_, iovecs, vlen, offset));
}

xe_promise xe_file::writev(const iovec* iovecs, uint vlen, long offset){
	if(fd_ < 0)
		return xe_promise::done(XE_STATE);
	return loop_ -> run(xe_op::writev(fd_, iovecs, vlen, offset));
}

void xe_file::close(){
	if(fd_ >= 0){
		::close(fd_);

		fd_ = -1;
	}
}