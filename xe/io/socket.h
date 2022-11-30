#pragma once
#include <netdb.h>
#include "xstd/types.h"
#include "xutil/util.h"
#include "../loop.h"

class xe_socket;
class xe_socket_req : public xe_req{
private:
	static void complete(xe_req&, int, uint);

	xe_socket* socket;

	friend class xe_socket;
public:
	typedef void (*xe_callback)(xe_socket_req& req, int result);

	xe_callback callback;

	xe_socket_req(xe_callback cb){
		event = complete;
		callback = cb;
	}

	xe_socket_req(): xe_socket_req(null){}

	~xe_socket_req() = default;
};

class xe_socket_promise : public xe_promise{
private:
	static void complete(xe_req& req, int, uint);

	xe_socket* socket;

	xe_socket_promise();
	xe_socket_promise(xe_socket_promise&&) = default;

	friend class xe_socket;
public:
	~xe_socket_promise() = default;
};

class xe_connect_req : public xe_req{
private:
	static void complete(xe_req&, int, uint);

	xe_socket* socket;

	friend class xe_socket;
public:
	typedef void (*xe_callback)(xe_connect_req& req, int result);

	xe_callback callback;

	xe_connect_req(xe_callback cb = null){
		event = complete;
		callback = cb;
	}

	~xe_connect_req() = default;
};

class xe_connect_promise : public xe_promise{
private:
	static void complete(xe_req& req, int, uint);

	xe_socket* socket;

	xe_connect_promise();
	xe_connect_promise(xe_connect_promise&&) = default;

	friend class xe_socket;
public:
	~xe_connect_promise() = default;
};

class xe_socket{
private:
	void open(int);
	void connect(int);

	xe_loop* loop_;

	int fd_;
	uint state;

	friend class xe_socket_req;
	friend class xe_socket_promise;
	friend class xe_connect_req;
	friend class xe_connect_promise;
public:
	xe_socket(){
		fd_ = -1;
		state = 0;
	}

	xe_socket(xe_loop& loop): xe_socket(){
		loop_ = &loop;
	}

	xe_disable_copy_move(xe_socket)

	void set_loop(xe_loop& loop){
		loop_ = &loop;
	}

	xe_loop& loop() const{
		return *loop_;
	}

	int fd() const{
		return fd_;
	}

	int init_sync(int af, int type, int proto);
	int init_fd(int fd);
	int accept(int fd);

	int init(xe_socket_req& req, int af, int type, int proto);
	xe_socket_promise init(int af, int type, int proto);

	int accept_sync(sockaddr* addr, socklen_t* addrlen, uint flags);
	int connect_sync(const sockaddr* addr, socklen_t addrlen);

	int accept(xe_req& req, sockaddr* addr, socklen_t* addrlen, uint flags);
	int connect(xe_connect_req& req, const sockaddr* addr, socklen_t addrlen);

	xe_promise accept(sockaddr* addr, socklen_t* addrlen, uint flags);
	xe_connect_promise connect(const sockaddr* addr, socklen_t addrlen);

	int recv_sync(xe_ptr buf, uint len, uint flags);
	int send_sync(xe_cptr buf, uint len, uint flags);

	int recv(xe_req& req, xe_ptr buf, uint len, uint flags);
	int send(xe_req& req, xe_cptr buf, uint len, uint flags);

	xe_promise recv(xe_ptr buf, uint len, uint flags);
	xe_promise send(xe_cptr buf, uint len, uint flags);

	int recvmsg_sync(msghdr* msg, uint flags);
	int sendmsg_sync(const msghdr* msg, uint flags);

	int recvmsg(xe_req& req, msghdr* msg, uint flags);
	int sendmsg(xe_req& req, const msghdr* msg, uint flags);

	xe_promise recvmsg(msghdr* msg, uint flags);
	xe_promise sendmsg(const msghdr* msg, uint flags);

	int shutdown_sync(int how);
	int shutdown(xe_req& req, int how);
	xe_promise shutdown(int how);

	int bind(sockaddr* addr, socklen_t addrlen);
	int listen(int maxqueuesize);

	void close();

	~xe_socket() = default;
};