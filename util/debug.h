#pragma once
#include "xe/types.h"

#define XE_INFO 0
#define XE_WARN 1
#define XE_ERROR 2
#define XE_TRACE 3

void xe_debug_print		(uint type, xe_cstr file, uint line, xe_cstr str, ...);
void xe_debug_assert	(xe_cstr file, uint line, intmax_t val, xe_cstr expr);
void xe_debug_asserteq	(xe_cstr file, uint line, intmax_t got, intmax_t expect);
void xe_debug_assertm	(xe_cstr file, uint line, intmax_t val, xe_cstr expr, xe_cstr message);
void xe_debug_asserteqm	(xe_cstr file, uint line, intmax_t got, intmax_t expect, xe_cstr message);
void xe_debug_stop		(xe_cstr file, uint line, xe_cstr message);

#define XE__FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
#define XE_LOC XE__FILENAME__, __LINE__

#define xe_log_warn(str, ...)	xe_debug_print(XE_WARN, XE_LOC, str, ##__VA_ARGS__)
#define xe_log_error(str, ...)	xe_debug_print(XE_ERROR, XE_LOC, str, ##__VA_ARGS__)

#ifdef XE_DEBUG
#define xe_log_trace(str, ...)	xe_debug_print(XE_TRACE, XE_LOC, str, ##__VA_ARGS__)

#define xe_assert(expr) 		xe_debug_assert		(XE_LOC, (expr), #expr)
#define xe_asserteq(g, e) 		xe_debug_asserteq	(XE_LOC, (intmax_t)(g), (intmax_t)(e))
#define xe_assertm(expr, msg) 	xe_debug_assertm	(XE_LOC, (expr), #expr, msg)
#define xe_asserteqm(g, e, msg)	xe_debug_asserteqm	(XE_LOC, (intmax_t)(g), (intmax_t)(e), msg)
#define xe_assertz(expr) 		xe_assert			((expr) == 0)
#define xe_assertzm(expr, msg) 	xe_assertm			((expr) == 0, msg)
#else
#define xe_log_trace(str, ...)

#define xe_assert(expr) 		(expr)
#define xe_asserteq(g, e) 		((g), (e))
#define xe_assertm(expr, msg) 	(expr)
#define xe_asserteqm(g, e, msg) ((g), (e))
#define xe_assertz(expr) 		(expr)
#define xe_assertzm(expr, msg) 	(expr)
#endif

#define xe_print(str, ...) 			xe_debug_print(XE_INFO, XE_LOC, str, ##__VA_ARGS__)

#define xe_stop(str) 				xe_debug_stop(XE_LOC, str)
#define xe_notreached 				xe_stop("not reached")