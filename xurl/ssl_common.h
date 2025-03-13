#pragma once
#include "ssl.h"

void xe_ssl_msg_callback(int direction, int version, int content_type, xe_cptr buf, size_t len, xe_ptr user);