#pragma once
#include "../loop.h"

class xe_file;
class xe_open_req : public xe_req{
private:
	static void complete(xe_req&, int, uint);

	xe_file* file;

	friend class xe_file;
public:
	typedef void (*xe_callback)(xe_open_req& req, int result);

	xe_callback callback;

	xe_open_req(xe_callback cb){
		event = complete;
		callback = cb;
	}

	xe_open_req(): xe_open_req(null){}

	xe_disallow_copy_move(xe_open_req)

	~xe_open_req() = default;
};

class xe_open_promise : public xe_promise{
private:
	static void complete(xe_req& req, int, uint);

	xe_file* file;

	xe_open_promise();
	xe_open_promise(xe_open_promise&&) = default;

	friend class xe_file;
public:
	~xe_open_promise() = default;
};

class xe_file{
private:
	void open(int);

	xe_loop* loop_;

	int fd_;
	bool opening: 1;

	friend class xe_open_req;
	friend class xe_open_promise;
public:
	xe_file(){
		fd_ = -1;
		opening = false;
	}

	xe_file(xe_loop& loop): xe_file(){
		loop_ = &loop;
	}

	xe_disallow_copy_move(xe_file)

	void set_loop(xe_loop& loop){
		loop_ = &loop;
	}

	xe_loop& loop() const{
		return *loop_;
	}

	int fd() const{
		return fd_;
	}

	int open(xe_open_req& req, xe_cstr path, uint flags);
	int openat(xe_open_req& req, int dfd, xe_cstr path, uint flags);
	xe_open_promise open(xe_cstr path, uint flags);
	xe_open_promise openat(int dfd, xe_cstr path, uint flags);

	int read(xe_req& req, xe_ptr buf, uint len, long offset);
	int write(xe_req& req, xe_cptr buf, uint len, long offset);
	xe_promise read(xe_ptr buf, uint len, long offset);
	xe_promise write(xe_cptr buf, uint len, long offset);

	void close();

	~xe_file() = default;
};