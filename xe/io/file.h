#pragma once
#include "../loop.h"

class xe_file{
private:
	xe_loop& loop_;

	int fd_;

	uint opening: 1;
	uint closing: 1;
	uint flags: 30;
	uint handle;
	uint pad;

	static void io(xe_loop_handle&, int);

	friend class xe_loop;
public:
	typedef void (*xe_callback)(xe_file& file, ulong key, int result);

	/* user offset pointer */
	ulong offset;

	xe_callback open_callback;
	xe_callback read_callback;
	xe_callback write_callback;

	xe_file(xe_loop& loop);

	int fd();

	xe_loop& loop();

	int open(xe_cstr path, uint flags);

	int read(xe_buf buf, uint len, ulong offset, ulong key = 0);
	int write(xe_buf buf, uint len, ulong offset, ulong key = 0);

	int cancelopen();
	void close();
};