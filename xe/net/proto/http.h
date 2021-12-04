#pragma once
#include "../protocol.h"

namespace xe_net{

enum xe_http_options{
	XE_HTTP_NONE = 0,
	XE_HTTP_HEADER,
	XE_HTTP_METHOD,
	XE_HTTP_CALLBACK_RESPONSE = XE_REQUEST_CALLBACK_LAST,
	XE_HTTP_CALLBACK_HEADER
};

enum xe_http_option_flags{
	XE_HTTP_FLAG_NONE = 0,
	XE_HTTP_CLONE_STRING
};

xe_protocol* xe_http_new(xe_net_ctx& net);

}