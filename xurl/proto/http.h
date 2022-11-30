#pragma once
#include "xstd/types.h"
#include "xstd/map.h"
#include "xstd/vector.h"
#include "http_base.h"

namespace xurl{

enum xe_http_version{
	XE_HTTP_VERSION_0_9 = 9,
	XE_HTTP_VERSION_1_0 = 10,
	XE_HTTP_VERSION_1_1 = 11,
	XE_HTTP_VERSION_2_0 = 20,
	XE_HTTP_VERSION_3_0 = 30
};

class xe_http_headers : public xe_map<xe_string, xe_vector<xe_string>, xe_http_lowercase_hash, xe_http_case_insensitive>{
public:
	xe_http_headers() = default;
	xe_http_headers(xe_http_headers&& other): xe_map(std::move(other)){}
	xe_http_headers& operator=(xe_http_headers&& other){
		xe_map::operator=(std::move(other));

		return *this;
	}

	xe_disable_copy(xe_http_headers)

	bool has(const xe_string_view& key){
		return xe_map::has((const xe_string&)key);
	}

	iterator find(const xe_string_view& key){
		return xe_map::find((const xe_string&)key);
	}

	~xe_http_headers() = default;
};

class xe_http_response{
private:
	void move(xe_http_response&& other){
		headers = std::move(other.headers);
		status_text = std::move(other.status_text);
		version = other.version;
		status = other.status;
	}
public:
	typedef typename xe_http_headers::iterator iterator;
	typedef typename xe_http_headers::const_iterator const_iterator;

	xe_http_headers headers;
	xe_string status_text;
	xe_http_version version;
	uint status;

	xe_http_response() = default;

	xe_http_response(xe_http_response&& other){
		move(std::move(other));
	}

	xe_http_response& operator=(xe_http_response&& other){
		move(std::move(other));

		return *this;
	}

	xe_disable_copy(xe_http_response)

	void clear(){
		headers.clear();
		status_text.clear();
	}

	~xe_http_response() = default;
};

class xe_http_input{
public:
	virtual bool readable();
	virtual int read(xe_ptr& buf, size_t& size);
};

typedef int (*xe_http_statusline_cb)(xe_request& request, xe_http_version version, uint status, const xe_string_view& reason);
typedef int (*xe_http_singleheader_cb)(xe_request& request, const xe_string_view& key, const xe_string_view& value);
typedef int (*xe_http_response_cb)(xe_request& request, xe_http_response& response);
typedef int (*xe_http_external_redirect_cb)(xe_request& request, const xe_url& url);

class xe_http_specific : public xe_http_common_data{
public:
	xe_http_specific();

	void set_min_version(xe_http_version version);
	void set_max_version(xe_http_version version);

	void set_body(xe_ptr data, size_t len);
	void set_input(xe_http_input& input);

	void set_statusline_cb(xe_http_statusline_cb cb);
	void set_singleheader_cb(xe_http_singleheader_cb cb);
	void set_response_cb(xe_http_response_cb cb);
	void set_trailer_cb(xe_http_singleheader_cb cb);

	bool set_method(const xe_string_view& method, uint flags = 0);

	~xe_http_specific() = default;
};

xe_protocol* xe_http_new(xurl_ctx& ctx);

}