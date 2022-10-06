#pragma once
#include "xstd/types.h"

namespace xurl{

enum xe_ip_mode : byte{
	XE_IP_ANY = 0,
	XE_IP_ONLY_V4,
	XE_IP_ONLY_V6,
	XE_IP_PREFER_V4,
	XE_IP_PREFER_V6
};

enum xe_transferctl_flags{
	XE_PAUSE_SEND = 0x1,
	XE_PAUSE_RECV = 0x2,
	XE_PAUSE_ALL = 0x3,
	XE_RESUME_SEND = 0x4,
	XE_RESUME_RECV = 0x8,
	XE_RESUME_ALL = 0xc
};

class xurl_ctx;
class xe_connection;
class xe_protocol;
class xe_protocol_specific;
class xe_request;
class xe_request_internal;
class xe_ssl_ctx;
class xe_url;

int xurl_init();
void xurl_cleanup();

}