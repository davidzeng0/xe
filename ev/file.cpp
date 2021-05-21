#include <fcntl.h>
#include <unistd.h>
#include "file.h"
#include "xe/error.h"
#include "xe/debug.h"
#include "xe/common.h"

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

void xe_file_init(xe_loop* loop, xe_file* file){
	file -> fd = -1;
	file -> loop = loop;
}

int xe_file_open(xe_file* file, xe_cstr path, uint flags){
	if(file -> flags & XE_FILE_OPENING || file -> fd >= 0)
		return XE_EINVAL;
	int ret = xe_loop_openat(file -> loop, AT_FDCWD, path, flags, 0, XE_LOOP_FILE, file, null, XE_FILE_OPEN, 0);

	if(ret >= 0){
		file -> flags |= XE_FILE_OPENING;
		file -> cancel = ret;
	}

	return ret;
}

int xe_file_read(xe_file* file, xe_buf buf, uint len, ulong offset){
	return xe_file_read(file, buf, len, offset, 0);
}

int xe_file_write(xe_file* file, xe_buf buf, uint len, ulong offset){
	return xe_file_write(file, buf, len, offset, 0);
}

int xe_file_read(xe_file* file, xe_buf buf, uint len, ulong offset, ulong key){
	if(file -> flags & XE_FILE_OPENING || file -> fd < 0)
		return XE_EINVAL;
	return xe_loop_read(file -> loop, file -> fd, buf, len, offset, XE_LOOP_FILE, file, null, XE_FILE_READ, key);
}

int xe_file_write(xe_file* file, xe_buf buf, uint len, ulong offset, ulong key){
	if(file -> flags & XE_FILE_OPENING || file -> fd < 0)
		return XE_EINVAL;
	return xe_loop_write(file -> loop, file -> fd, buf, len, offset, XE_LOOP_FILE, file, null, XE_FILE_WRITE, key);
}

int xe_file_cancelopen(xe_file* file){
	if(!(file -> flags & XE_FILE_OPENING) || (file -> flags & XE_FILE_CLOSING))
		return XE_EINVAL;
	int ret = xe_loop_cancel(file -> loop, file -> cancel, 0, XE_LOOP_DISCARD, null, null, 0, 0);

	if(ret >= 0){
		file -> flags |= XE_FILE_CLOSING;

		return 0;
	}

	return ret;
}

void xe_file_close(xe_file* file){
	if(file -> fd >= 0){
		close(file -> fd);

		file -> fd = -1;
	}
}

void xe_loop_file(xe_file* file, xe_handle* handle, int result){
	switch(handle -> u1){
		case XE_FILE_OPEN:
			if(result >= 0){
				if(file -> flags & XE_FILE_CLOSING)
					close(result);
				else
					file -> fd = result;
				result = 0;
			}

			file -> flags &= ~(XE_FILE_OPENING | XE_FILE_CLOSING);
			file -> open_callback(file, 0, result);

			break;
		case XE_FILE_READ:
			file -> read_callback(file, handle -> u2, result);

			break;
		case XE_FILE_WRITE:
			file -> write_callback(file, handle -> u2, result);

			break;
		default:
			xe_notreached;
	}
}