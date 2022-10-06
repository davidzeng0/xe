#pragma once
#include "xconfig/config.h"
#include "xstd/types.h"
#include "source.h"

void xe__assertfail(xe_cstr file, uint line, xe_cstr expr);
void xe__assertfailm(xe_cstr file, uint line, xe_cstr expr, xe_cstr message);

void xe__asserteqfail(xe_cstr file, uint line, intmax_t a, intmax_t b);
void xe__asserteqfailm(xe_cstr file, uint line, intmax_t a, intmax_t b, xe_cstr message);

void xe__stop(xe_cstr file, uint line, xe_cstr message);

#ifdef XE_DEBUG
	#define xe_assert(expr)			((expr) ? static_cast<void>(0) : xe__assertfail(XE_SOURCE, #expr))
	#define xe_assertm(expr, msg)	((expr) ? static_cast<void>(0) : xe__assertfailm(XE_SOURCE, #expr, msg))
	#define xe_asserteq(a, b)		((a)== (b) ? static_cast<void>(0) : xe__asserteqfail(XE_SOURCE, a, b))
	#define xe_asserteqm(a, b, msg)	((a)== (b) ? static_cast<void>(0) : xe__asserteqfailm(XE_SOURCE, a, b, msg))
	#define xe_assertz(expr)		xe_assert		((expr) == 0)
	#define xe_assertzm(expr, msg)	xe_assertm		((expr) == 0, msg)
#else
	#define xe_assert(expr)			((void)(expr))
	#define xe_asserteq(g, e)		((void)((g) == (e)))
	#define xe_assertm(expr, msg)	((void)(expr))
	#define xe_asserteqm(g, e, msg)	((void)((g) == (e)))
	#define xe_assertz(expr)		((void)(expr))
	#define xe_assertzm(expr, msg)	((void)(expr))
#endif

#define xe_stop(str)				xe__stop(XE_SOURCE, str)
#define xe_notreached()				xe_stop("Not reached")