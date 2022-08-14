#include <fcntl.h>
#include <unistd.h>
#include "file.h"
#include "xutil/assert.h"
#include "../error.h"

enum xe_file_iotype{
	XE_FILE_OPEN = 0,
	XE_FILE_READ,
	XE_FILE_WRITE
};

xe_file::xe_file(xe_loop& loop): loop_(loop){
	fd_ = -1;
	handle = -1;
	opening = 0;
	closing = 0;
	flags = 0;
}

int xe_file::fd(){
	return fd_;
}

xe_loop& xe_file::loop(){
	return loop_;
}

int xe_file::open(xe_cstr path, uint flags){
	if(opening)
		return XE_EALREADY;
	if(fd_ >= 0)
		return XE_STATE;
	int ret = loop_.openat(AT_FDCWD, path, flags, 0, this, null, XE_FILE_OPEN, 0, XE_LOOP_HANDLE_FILE);

	if(ret >= 0){
		opening = true;
		handle = ret;
	}

	return ret;
}

int xe_file::read(xe_ptr buf, uint len, ulong offset, ulong key){
	if(opening || fd_ < 0)
		return XE_STATE;
	return loop_.read(fd_, buf, len, offset, this, null, XE_FILE_READ, key, XE_LOOP_HANDLE_FILE);
}

int xe_file::write(xe_cptr buf, uint len, ulong offset, ulong key){
	if(opening || fd_ < 0)
		return XE_STATE;
	return loop_.write(fd_, buf, len, offset, this, null, XE_FILE_WRITE, key, XE_LOOP_HANDLE_FILE);
}

int xe_file::cancelopen(){
	if(!opening)
		return XE_ENOENT;
	if(closing)
		return XE_EALREADY;
	int ret = loop_.cancel(handle, 0, null, null, 0, 0, XE_LOOP_HANDLE_DISCARD);

	if(ret >= 0){
		closing = true;

		return 0;
	}

	return ret;
}

void xe_file::close(){
	if(fd_ >= 0){
		::close(fd_);

		fd_ = -1;
	}
}

void xe_file::io(xe_loop_handle& handle, int result){
	xe_file& file = *(xe_file*)handle.user_data;

	switch(handle.u1){
		case XE_FILE_OPEN:
			if(result >= 0){
				if(file.closing)
					::close(result);
				else
					file.fd_ = result;
				result = 0;
			}

			file.opening = false;
			file.closing = false;
			file.open_callback(file, 0, result);

			break;
		case XE_FILE_READ:
			file.read_callback(file, handle.u2, result);

			break;
		case XE_FILE_WRITE:
			file.write_callback(file, handle.u2, result);

			break;
		default:
			xe_notreached();

			break;
	}
}