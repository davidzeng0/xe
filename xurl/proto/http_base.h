#pragma once
#include "net_common.h"
#include "xe/string.h"
#include "xe/container/map.h"

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

	bool set_header(xe_string key, xe_string value, uint flags = 0);

	xe_string location();

	~xe_http_common_data(){}
};

}