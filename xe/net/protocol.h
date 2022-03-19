#pragma once
#include "../types.h"
#include "url.h"

namespace xe_net{

class xe_protocol_data;
class xe_protocol;
class xe_connection_handler;

enum xe_protocol_id{
	XE_PROTOCOL_NONE = -1,
	XE_PROTOCOL_FILE,
	XE_PROTOCOL_HTTP,
	XE_PROTOCOL_WEBSOCKET,

	XE_PROTOCOL_LAST
};

}

#include "net.h"
#include "request.h"

namespace xe_net{

class xe_protocol_data{
protected:
	xe_protocol_id id_;

	xe_protocol_data(xe_protocol_id id){
		id_ = id;
	}
public:
	virtual int set(int option, xe_ptr value, int flags) = 0;
	virtual int set(int option, xe_ptr value1, xe_ptr value2, int flags) = 0;

	virtual ~xe_protocol_data(){}

	xe_protocol_id id(){
		return id_;
	}
};

class xe_protocol{
protected:
	xe_protocol_id id_;
	xe_net_ctx& net;

	xe_protocol(xe_net_ctx& net, xe_protocol_id id): net(net){
		id_ = id;
	}
public:
	virtual int start(xe_request& request) = 0;
	virtual void pause(xe_request& request, bool paused) = 0;
	virtual void end(xe_request& request) = 0;

	virtual bool matches(const xe_string& scheme) const = 0;
	virtual int open(xe_request& request, xe_url url) = 0;

	virtual ~xe_protocol(){}

	xe_protocol_id id() const{
		return id_;
	}
};

}