#include "file.h"
#include "xe/error.h"
#include "xe/io/file.h"
#include "xutil/log.h"
#include "../url.h"
#include "../ctx.h"
#include "../request.h"
#include "../request_internal.h"

namespace xurl{

enum{
	XE_FILE_STREAM_BUFFER_SIZE = XE_LOOP_IOBUF_SIZE
};

class xe_file_stream;
class xe_file_data : public xe_protocol_specific{
public:
	xe_url url;
	xe_file_stream* stream;

	xe_file_data(): xe_protocol_specific(XE_PROTOCOL_FILE){}

	~xe_file_data() = default;
};

class xe_file_stream{
public:
	xe_file file;
	xe_file_data* data;
	xe_request_internal* request;

	xe_string path;
	xe_open_req open;
	xe_req read;
	long offset;

	byte* buf;

	static void open_callback(xe_open_req& req, int result){
		xe_file_stream& stream = xe_containerof(req, &xe_file_stream::open);

		if(!result)
			result = stream.file.read(stream.read, stream.buf, XE_FILE_STREAM_BUFFER_SIZE, stream.offset);
		if(result)
			stream.complete(result);
		else
			stream.request -> set_state(XE_REQUEST_STATE_ACTIVE);
	}

	static void read_callback(xe_req& req, int result){
		xe_file_stream& stream = xe_containerof(req, &xe_file_stream::read);

		do{
			if(result <= 0)
				break;
			if(stream.request -> write(stream.buf, result)){
				result = XE_ECANCELED;

				break;
			}

			stream.offset += result;
			result = stream.file.read(stream.read, stream.buf, XE_FILE_STREAM_BUFFER_SIZE, stream.offset);

			if(result)
				break;
			return;
		}while(false);

		stream.complete(result);
	}

	xe_file_stream(xe_loop& loop, xe_request_internal* request_){
		file.set_loop(loop);
		request = request_;
		data = (xe_file_data*)request -> data;

		open.callback = open_callback;
		read.callback = read_callback;
	}

	int start(){
		int err;

		buf = xe_alloc<byte>(XE_FILE_STREAM_BUFFER_SIZE);

		if(!buf)
			return XE_ENOMEM;
		if(!path.copy(data -> url.path()))
			return XE_ENOMEM;
		err = file.open(open, path.c_str(), O_RDONLY);

		if(!err)
			request -> set_state(XE_REQUEST_STATE_CONNECTING);
		return err;
	}

	void end(){

	}

	void complete(int res){
		request -> complete(res);
	}

	~xe_file_stream(){
		xe_dealloc(buf);
	}
};

class xe_file_protocol : public xe_protocol{
public:
	xe_file_protocol(xurl_ctx& ctx): xe_protocol(ctx, XE_PROTOCOL_FILE){}

	int start(xe_request_internal& request);

	int transferctl(xe_request_internal& request, uint flags);
	void end(xe_request_internal& request);

	int open(xe_request_internal& request, xe_url&& url);

	bool matches(const xe_string_view& scheme) const;

	~xe_file_protocol() = default;

	static xe_cstr class_name();
};

int xe_file_protocol::start(xe_request_internal& request){
	xe_file_stream* stream = xe_znew<xe_file_stream>(ctx -> loop(), &request);
	int err;

	if(!stream)
		return XE_ENOMEM;
	err = stream -> start();

	if(err)
		xe_delete(stream);
	return err;
}

int xe_file_protocol::transferctl(xe_request_internal& request, uint flags){
	return 0;
}

void xe_file_protocol::end(xe_request_internal& request){
	auto& data = *(xe_file_data*)request.data;

	data.stream -> end();
}

int xe_file_protocol::open(xe_request_internal& request, xe_url&& url){
	xe_file_data* data;

	if(request.data && request.data -> id() == XE_PROTOCOL_FILE){
		data = (xe_file_data*)request.data;
	}else{
		data = xe_new<xe_file_data>();

		if(!data) return XE_ENOMEM;
	}

	data -> url = std::move(url);
	data -> stream = null;
	request.data = data;

	xe_log_verbose(this, "opened file request for: %s", data -> url.href().data());

	return 0;
}

bool xe_file_protocol::matches(const xe_string_view& scheme) const{
	return scheme == "file";
}

xe_cstr xe_file_protocol::class_name(){
	return "xe_file";
}

xe_protocol* xe_file_new(xurl_ctx& ctx){
	return xe_new<xe_file_protocol>(ctx);
}

}