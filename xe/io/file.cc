#include <fcntl.h>
#include <unistd.h>
#include "file.h"
#include "../error.h"
#include "../log.h"
#include "../common.h"

enum xe_file_flags{
	XE_FILE_NONE    = 0x0,
	XE_FILE_OPENING = 0x1,
	XE_FILE_CLOSING = 0x2
};

enum xe_file_iotype{
	XE_FILE_OPEN = 0,
	XE_FILE_READ,
	XE_FILE_WRITE
};

xe_file::xe_file(xe_loop& loop): loop(loop){
	fd = -1;
	handle = -1;
	flags = 0;
}

int xe_file::get_fd(){
	return fd;
}

xe_loop& xe_file::get_loop(){
	return loop;
}

int xe_file::open(xe_cstr path, uint flags){
	if(flags & XE_FILE_OPENING || fd >= 0)
		return XE_EINVAL;
	int ret = loop.openat(AT_FDCWD, path, flags, 0, this, null, XE_FILE_OPEN, 0, XE_HANDLE_FILE);

	if(ret >= 0){
		flags |= XE_FILE_OPENING;
		handle = ret;
	}

	return ret;
}

int xe_file::read(xe_buf buf, uint len, ulong offset, ulong key){
	if(flags & XE_FILE_OPENING || fd < 0)
		return XE_EINVAL;
	return loop.read(fd, buf, len, offset, this, null, XE_FILE_READ, key, XE_HANDLE_FILE);
}

int xe_file::write(xe_buf buf, uint len, ulong offset, ulong key){
	if(flags & XE_FILE_OPENING || fd < 0)
		return XE_EINVAL;
	return loop.write(fd, buf, len, offset, this, null, XE_FILE_WRITE, key, XE_HANDLE_FILE);
}

int xe_file::cancelopen(){
	if(!(flags & XE_FILE_OPENING) || (flags & XE_FILE_CLOSING))
		return XE_EINVAL;
	int ret = loop.cancel(handle, 0, null, null, 0, 0, XE_HANDLE_DISCARD);

	if(ret >= 0){
		flags |= XE_FILE_CLOSING;

		return 0;
	}

	return ret;
}

void xe_file::close(){
	if(fd >= 0){
		::close(fd);

		fd = -1;
	}
}

void xe_file::io(xe_handle& handle, int result){
	xe_file& file = *(xe_file*)handle.user_data;

	switch(handle.u1){
		case XE_FILE_OPEN:
			if(result >= 0){
				if(file.flags & XE_FILE_CLOSING)
					::close(result);
				else
					file.fd = result;
				result = 0;
			}

			file.flags &= ~(XE_FILE_OPENING | XE_FILE_CLOSING);
			file.open_callback(file, 0, result);

			break;
		case XE_FILE_READ:
			file.read_callback(file, handle.u2, result);

			break;
		case XE_FILE_WRITE:
			file.write_callback(file, handle.u2, result);

			break;
		default:
			xe_notreached;
	}
}