#pragma once
#include "../../types.h"

namespace xe_net{

enum xe_net_options{
	XE_NET_NONE = 0,
	XE_NET_MAX_REDIRECT,
	XE_NET_CONNECT_TIMEOUT,
	XE_NET_RECVBUF_SIZE,
	XE_NET_PORT,
	XE_NET_SSL_VERIFY,
	XE_NET_FOLLOW_LOCATION,
	XE_NET_IP_MODE,
	XE_NET_LAST
};

enum xe_ip_mode : byte{
	XE_IP_ANY = 0,
	XE_IP_ONLY_V4,
	XE_IP_ONLY_V6,
	XE_IP_PREFER_V4,
	XE_IP_PREFER_V6
};

struct xe_net_data{
	uint max_redirects;
	uint connect_timeout;
	uint recvbuf_size;
	ushort port;
	xe_ip_mode ip_mode;

	byte ssl_verify: 1;
	byte follow_location: 1;
	byte pad: 5;
};

}