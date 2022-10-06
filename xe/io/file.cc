#include <unistd.h>
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

int xe_file::open(xe_open_req& req, xe_cstr path, uint flags){
	return openat(req, AT_FDCWD, path, flags);
}

int xe_file::openat(xe_open_req& req, int dfd, xe_cstr path, uint flags){
	if(opening)
		return XE_EALREADY;
	if(fd_ >= 0)
		return XE_STATE;
	xe_return_error(loop_ -> openat(req, dfd, path, flags, 0));

	req.file = this;
	opening = true;

	return 0;
}

xe_open_promise xe_file::open(xe_cstr path, uint flags){
	return openat(AT_FDCWD, path, flags);
}

xe_open_promise xe_file::openat(int dfd, xe_cstr path, uint flags){
	xe_open_promise promise;
	int res;

	if(opening)
		res = XE_EALREADY;
	else if(fd_ >= 0)
		res = XE_STATE;
	else
		res = loop_ -> openat(promise, dfd, path, flags, 0);
	if(res){
		promise.result_ = res;
		promise.ready_ = true;
	}else{
		promise.file = this;
		opening = true;
	}

	return promise;
}

int xe_file::read(xe_req& req, xe_ptr buf, uint len, long offset){
	if(fd_ < 0)
		return XE_STATE;
	return loop_ -> read(req, fd_, buf, len, offset);
}

int xe_file::write(xe_req& req, xe_cptr buf, uint len, long offset){
	if(fd_ < 0)
		return XE_STATE;
	return loop_ -> write(req, fd_, buf, len, offset);
}

xe_promise xe_file::read(xe_ptr buf, uint len, long offset){
	if(fd_ < 0)
		return xe_promise::done(XE_STATE);
	return loop_ -> read(fd_, buf, len, offset);
}

xe_promise xe_file::write(xe_cptr buf, uint len, long offset){
	if(fd_ < 0)
		return xe_promise::done(XE_STATE);
	return loop_ -> write(fd_, buf, len, offset);
}

void xe_file::close(){
	if(fd_ >= 0){
		::close(fd_);

		fd_ = -1;
	}
}