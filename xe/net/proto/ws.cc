#include "ws.h"

using namespace xe_net;

class xe_websocket: public xe_protocol{
public:
	xe_websocket(xe_net_ctx& net): xe_protocol(net, XE_PROTOCOL_WEBSOCKET){

	}

	int start(xe_request& request){
		return XE_EINVAL;
	}

	void pause(xe_request& request, bool paused){

	}

	void end(xe_request& request){

	}

	int open(xe_request& request, xe_url url){
		return 0;
	}

	bool matches(const xe_string& scheme) const{
		return scheme == "ws" || scheme == "wss";
	}

	~xe_websocket(){

	}
};

xe_protocol* xe_net::xe_websocket_new(xe_net_ctx& net){
	return xe_new<xe_websocket>(net);
}