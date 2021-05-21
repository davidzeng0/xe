#pragma once
#include <errno.h>
#include "xe/types.h"

static constexpr int xe_syserror(int err){
	return -err;
}

enum xe_error{
	XE_FIRST = -32768,
	XE_ETIMERALREADYACTIVE,
	XE_ETIMERNOTACTIVE,
	XE_ETOOMANYHANDLES,
	XE_EINVAL,
	XE_LAST,
	XE_ECANCELED = xe_syserror(ECANCELED),
	XE_ENOMEM = xe_syserror(ENOMEM),
	XE_NONE = 0
};

static xe_cstr xe_strerror(int err){
	switch(err){
		case XE_NONE:
			return "OK";
		case XE_ENOMEM:
			return "No memory";
		case XE_EINVAL:
			return "Invalid argument";
		case XE_ETIMERALREADYACTIVE:
			return "Timer already active";
		case XE_ETIMERNOTACTIVE:
			return "Timer not active";
		case XE_ETOOMANYHANDLES:
			return "Too many I/O handles";
		case XE_ECANCELED:
			return "Operation canceled";
		default:
			if(err > XE_LAST)
				return "System error";
			return "Unknown error";
	}
}