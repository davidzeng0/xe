#pragma once
#include "../protocol.h"
#include "net_common.h"

namespace xe_net{

enum xe_http_options{
	XE_HTTP_HEADER = XE_NET_LAST,
	XE_HTTP_METHOD,
	XE_HTTP_CALLBACK_RESPONSE,
	XE_HTTP_CALLBACK_HEADER
};

enum xe_http_option_flags{
	XE_HTTP_FLAG_NONE = 0,
	XE_HTTP_CLONE_STRING
};

typedef int (*xe_http_response_cb)(xe_request& request, uint status, xe_string& reason);
typedef int (*xe_http_header_cb)(xe_request& request, xe_string& key, xe_string& value);

xe_protocol* xe_http_new(xe_net_ctx& net);

}