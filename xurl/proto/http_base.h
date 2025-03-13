#pragma once
#include "net_common.h"
#include "xstd/string.h"

namespace xurl{

enum xe_http_option_flags{
	XE_HTTP_NONE = 0x0,
	XE_HTTP_COPY_KEY = 0x1,
	XE_HTTP_FREE_KEY = 0x2,
	XE_HTTP_COPY_VALUE = 0x4,
	XE_HTTP_FREE_VALUE = 0x8,
	XE_HTTP_COPY = 0x5,
	XE_HTTP_FREE = 0xa
};

struct xe_http_case_insensitive{
	bool operator()(const xe_string_view& a, const xe_string_view& b) const;
};

struct xe_http_lowercase_hash{
	size_t operator()(const xe_string_view& str) const;
};

class xe_http_common_data : public xe_net_common_data{
protected:
	bool follow_location: 1;
	bool immediate_redirect: 1;
	uint max_redirects;

	xe_http_common_data(xe_protocol_id id): xe_net_common_data(id){
		follow_location = false;
		max_redirects = 5;
	}
public:
	void set_max_redirects(uint max_redirects_){
		max_redirects = max_redirects_;
	}

	uint get_max_redirects(){
		return max_redirects;
	}

	void set_follow_location(bool follow_location_){
		follow_location = follow_location_;
	}

	bool get_follow_location(){
		return follow_location;
	}

	void set_immediate_redirect(bool immediate_redirect_){
		immediate_redirect = immediate_redirect_;
	}

	bool get_immediate_redirect(){
		return immediate_redirect;
	}

	bool set_header(const xe_string_view& key, const xe_string_view& value, uint flags = 0);

	xe_string_view location() const;

	~xe_http_common_data() = default;
};

}