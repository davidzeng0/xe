#pragma once
#include "../loop.h"

class xe_file{
private:
	xe_loop& loop;

	int fd;

	uint flags;
	uint handle;
	uint pad;

	static void io(xe_handle&, int);

	friend class xe_loop;
public:
	typedef void (*xe_callback)(xe_file& file, ulong key, int result);

	/* user offset pointer */
	ulong offset;

	xe_callback open_callback;
	xe_callback read_callback;
	xe_callback write_callback;

	xe_file(xe_loop& loop);

	int get_fd();

	xe_loop& get_loop();

	int open(xe_cstr path, uint flags);

	int read(xe_buf buf, uint len, ulong offset, ulong key = 0);
	int write(xe_buf buf, uint len, ulong offset, ulong key = 0);

	int cancelopen();
	void close();
};