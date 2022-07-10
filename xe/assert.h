#pragma once
#include "log.h"

void xe__assert		(xe_cstr file, uint line, intmax_t val, xe_cstr expr);
void xe__assertm	(xe_cstr file, uint line, intmax_t val, xe_cstr expr, xe_cstr message);
void xe__asserteq	(xe_cstr file, uint line, intmax_t got, intmax_t expect);
void xe__asserteqm	(xe_cstr file, uint line, intmax_t got, intmax_t expect, xe_cstr message);
void xe__stop		(xe_cstr file, uint line, xe_cstr message);

#ifdef XE_DEBUG
	#define xe_assert(expr) 		xe__assert		(XE_SOURCE, (expr), #expr)
	#define xe_asserteq(g, e) 		xe__asserteq	(XE_SOURCE, (intmax_t)(g), (intmax_t)(e))
	#define xe_assertm(expr, msg) 	xe__assertm		(XE_SOURCE, (expr), #expr, msg)
	#define xe_asserteqm(g, e, msg)	xe__asserteqm	(XE_SOURCE, (intmax_t)(g), (intmax_t)(e), msg)
	#define xe_assertz(expr) 		xe_assert		((expr) == 0)
	#define xe_assertzm(expr, msg) 	xe_assertm		((expr) == 0, msg)
#else
	#define xe_assert(expr) 		((void)(expr))
	#define xe_asserteq(g, e) 		((void)((g) == (e)))
	#define xe_assertm(expr, msg) 	((void)(expr))
	#define xe_asserteqm(g, e, msg)	((void)((g) == (e)))
	#define xe_assertz(expr) 		((void)(expr))
	#define xe_assertzm(expr, msg) 	((void)(expr))
#endif

#define xe_stop(str) 				xe__stop(XE_SOURCE, str)
#define xe_notreached() 			xe_stop("Not reached")