#pragma once
#include <errno.h>
#include "xstd/types.h"

int xe_errno();
xe_cstr xe_strerror(int err);

static constexpr inline int xe_syserror(int err){
	return -err;
}

static constexpr inline int xe_unsyserror(int err){
	return -err;
}

#include "error_def.h"

static constexpr inline int xe_error_is_syserror(int err){
	return err > XE_LAST;
}