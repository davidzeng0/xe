#include <unistd.h>
#include <fcntl.h>
#include "file.h"
#include "../../io/file.h"

namespace xe_net{

class xe_file_data : public xe_protocol_data{
private:
	xe_url url;

	friend class xe_file_reader;
public:
	xe_file_data();

	int open(xe_url _url);
	int set(int option, xe_ptr value, int flags);
	int set(int option, xe_ptr value1, xe_ptr value2, int flags);

	~xe_file_data();
};

xe_file_data::xe_file_data(): xe_protocol_data(XE_PROTOCOL_FILE){

}

int xe_file_data::open(xe_url _url){
	url = _url;

	return 0;
}

int xe_file_data::set(int option, xe_ptr value, int flags){
	return XE_EOPNOTSUPP;
}

int xe_file_data::set(int option, xe_ptr value1, xe_ptr value2, int flags){
	return XE_EOPNOTSUPP;
}

xe_file_data::~xe_file_data(){

}

class xe_file : public xe_protocol{
public:
	xe_file(xe_net_ctx& net);

	int start(xe_request& request);

	void pause(xe_request& request, bool paused);
	void end(xe_request& request);

	int open(xe_request& request, xe_url url);

	bool matches(const xe_string& scheme) const;

	~xe_file();
};

xe_file::xe_file(xe_net_ctx& net) : xe_protocol(net, XE_PROTOCOL_FILE){

}

int xe_file::start(xe_request& request){
	return XE_ENOSYS;
}

void xe_file::pause(xe_request& request, bool paused){

}

void xe_file::end(xe_request& request){

}

int xe_file::open(xe_request& request, xe_url url){
	xe_file_data* data;

	if(request.data && request.data -> id() == XE_PROTOCOL_FILE){
		data = (xe_file_data*)request.data;
	}else{
		data = xe_new<xe_file_data>();

		if(!data)
			return XE_ENOMEM;
	}

	int err = data -> open(url);

	if(err){
		xe_delete(data);

		return err;
	}

	xe_log_trace("xe_file", this, "opened file request for: %s", url.string().c_str());

	request.data = data;

	return 0;
}

bool xe_file::matches(const xe_string& scheme) const{
	return scheme == "file";
}

xe_file::~xe_file(){

}

xe_protocol* xe_file_new(xe_net_ctx& net){
	return xe_new<xe_file>(net);
}

}