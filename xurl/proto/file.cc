#include <unistd.h>
#include <fcntl.h>
#include "file.h"
#include "xe/io/file.h"
#include "../request.h"
#include "../ctx.h"
#include "../request_internal.h"

namespace xurl{

class xe_file_data : public xe_protocol_specific{
private:
	xe_url url;

	friend class xe_file_reader;
public:
	xe_file_data();

	int open(xe_url&& url);

	~xe_file_data();
};

class xe_file_reader{
	::xe_file file;
	xe_file_data* data;
	xe_request_internal* request;
	xe_ptr buf;
	size_t len;

	static void open_callback(::xe_file& file, ulong unused, int result){
		xe_file_reader& reader = *(xe_file_reader*)&file;

		if(result){
			reader.request -> complete(result);

			return;
		}

		int handle = file.read(reader.buf, reader.len, file.offset);

		xe_assert(handle >= 0);
	}

	static void read_callback(::xe_file& file, ulong unused, int result){
		xe_file_reader& reader = *(xe_file_reader*)&file;

		if(result <= 0){
			reader.request -> complete(result);

			return;
		}

		reader.request -> write(reader.buf, result);
		file.offset += result;

		int handle = file.read(reader.buf, reader.len, file.offset);

		xe_assert(handle >= 0);
	}
public:
	xe_file_reader(xe_loop& loop, xe_request_internal* request_) : file(loop){
		request = request_;
		data = (xe_file_data*)request -> data;
		file.open_callback = open_callback;
		file.read_callback = read_callback;

		len = 16384;
		buf = xe_alloc<byte>(len);
	}

	int start(){
		xe_string_view path = data -> url.path();
		xe_string path_copy;

		if(!path_copy.copy(path))
			return XE_ENOMEM;
		int err = file.open(path_copy.c_str(), O_RDONLY);

		if(err < 0)
			return err;
		request -> set_state(XE_REQUEST_STATE_ACTIVE);

		return 0;
	}
};

xe_file_data::xe_file_data(): xe_protocol_specific(XE_PROTOCOL_FILE){

}

int xe_file_data::open(xe_url&& url_){
	url = std::move(url_);

	return 0;
}

xe_file_data::~xe_file_data(){

}

class xe_file : public xe_protocol{
public:
	xe_file(xurl_ctx& ctx);

	int start(xe_request_internal& request);

	int transferctl(xe_request_internal& request, uint flags);
	void end(xe_request_internal& request);

	int open(xe_request_internal& request, xe_url&& url);

	bool matches(const xe_string_view& scheme) const;

	~xe_file();

	static xe_cstr class_name();
};

xe_file::xe_file(xurl_ctx& ctx) : xe_protocol(ctx, XE_PROTOCOL_FILE){

}

int xe_file::start(xe_request_internal& request){
	xe_file_reader* reader = xe_znew<xe_file_reader>(ctx -> loop(), &request);

	if(!reader)
		return XE_ENOMEM;
	int err = reader -> start();

	if(err){
		xe_delete(reader);

		return err;
	}

	return 0;
}

int xe_file::transferctl(xe_request_internal& request, uint flags){
	return 0;
}

void xe_file::end(xe_request_internal& request){

}

int xe_file::open(xe_request_internal& request, xe_url&& url){
	xe_file_data* data;

	if(request.data && request.data -> id() == XE_PROTOCOL_FILE){
		data = (xe_file_data*)request.data;
	}else{
		data = xe_new<xe_file_data>();

		if(!data)
			return XE_ENOMEM;
	}

	int err = data -> open(std::move(url));

	if(err){
		xe_delete(data);

		return err;
	}

	xe_log_verbose(this, "opened file request for: %s", url.href().c_str());

	request.data = data;

	return 0;
}

bool xe_file::matches(const xe_string_view& scheme) const{
	return scheme == "file";
}

xe_file::~xe_file(){

}

xe_cstr xe_file::class_name(){
	return "xe_file";
}

xe_protocol* xe_file_new(xurl_ctx& ctx){
	return xe_new<xe_file>(ctx);
}

}