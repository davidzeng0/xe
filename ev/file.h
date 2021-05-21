#pragma once
#include "loop.h"

struct xe_file{
	typedef void (*xe_callback)(xe_file* file, ulong key, int result);

	xe_loop* loop;

	int fd;

	uint flags;
	uint cancel;
	uint pad;

	/* user offset pointer */
	ulong offset;

	xe_callback open_callback;
	xe_callback read_callback;
	xe_callback write_callback;
};

void xe_file_init(xe_loop* loop, xe_file* file);

int xe_file_open(xe_file* file, xe_cstr path, uint flags);

int xe_file_read(xe_file* file, xe_buf buf, uint len, ulong offset);
int xe_file_write(xe_file* file, xe_buf buf, uint len, ulong offset);
int xe_file_read(xe_file* file, xe_buf buf, uint len, ulong offset, ulong key);
int xe_file_write(xe_file* file, xe_buf buf, uint len, ulong offset, ulong key);

int xe_file_cancelopen(xe_file* file);
void xe_file_close(xe_file* file);

void xe_loop_file(xe_file*, xe_handle*, int);