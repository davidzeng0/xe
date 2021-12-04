#pragma once
#include "types.h"

#define XE_SOURCE (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__), __LINE__

enum xe_loglevel{
	XE_LOG_NONE,
	XE_LOG_ERROR,
	XE_LOG_WARN,
	XE_LOG_INFO,
	XE_LOG_DEBUG,
	XE_LOG_TRACE
};

void xe_log_setlevel(xe_loglevel level);

void xe__print		(uint type, xe_cstr file, uint line, xe_cstr str, ...);
void xe__log		(uint type, xe_cstr name, xe_cptr addr, xe_cstr str, ...);
void xe__assert		(xe_cstr file, uint line, intmax_t val, xe_cstr expr);
void xe__assertm	(xe_cstr file, uint line, intmax_t val, xe_cstr expr, xe_cstr message);
void xe__asserteq	(xe_cstr file, uint line, intmax_t got, intmax_t expect);
void xe__asserteqm	(xe_cstr file, uint line, intmax_t got, intmax_t expect, xe_cstr message);
void xe__stop		(xe_cstr file, uint line, xe_cstr message);

#define xe_print(...) 			xe__print(XE_LOG_INFO, XE_SOURCE, ##__VA_ARGS__)

#ifdef XE_DEBUG
	#define xe_log_info(...) 		xe__log(XE_LOG_INFO, ##__VA_ARGS__)
	#define xe_log_warn(...)		xe__log(XE_LOG_WARN, ##__VA_ARGS__)
	#define xe_log_error(...)		xe__log(XE_LOG_ERROR, ##__VA_ARGS__)
	#define xe_log_debug(...)		xe__log(XE_LOG_DEBUG, ##__VA_ARGS__)
	#define xe_log_trace(...)		xe__log(XE_LOG_TRACE, ##__VA_ARGS__)

	#define xe_assert(expr) 		xe__assert		(XE_SOURCE, (expr), #expr)
	#define xe_asserteq(g, e) 		xe__asserteq	(XE_SOURCE, (intmax_t)(g), (intmax_t)(e))
	#define xe_assertm(expr, msg) 	xe__assertm		(XE_SOURCE, (expr), #expr, msg)
	#define xe_asserteqm(g, e, msg)	xe__asserteqm	(XE_SOURCE, (intmax_t)(g), (intmax_t)(e), msg)
	#define xe_assertz(expr) 		xe_assert		((expr) == 0)
	#define xe_assertzm(expr, msg) 	xe_assertm		((expr) == 0, msg)
#else
	#define xe_log_info(...)
	#define xe_log_warn(...)
	#define xe_log_error(...)
	#define xe_log_debug(...)
	#define xe_log_trace(...)

	#define xe_assert(expr) 		((void)(expr))
	#define xe_asserteq(g, e) 		((void)((g) == (e)))
	#define xe_assertm(expr, msg) 	((void)(expr))
	#define xe_asserteqm(g, e, msg)	((void)((g) == (e)))
	#define xe_assertz(expr) 		((void)(expr))
	#define xe_assertzm(expr, msg) 	((void)(expr))
#endif

#define xe_stop(str) 				xe__stop(XE_SOURCE, str)
#define xe_notreached 				xe_stop("Not reached")