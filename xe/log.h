#pragma once
#include "types.h"
#include "xe/config.h"

#define XE_SOURCE (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__), __LINE__

enum xe_loglevel{
	XE_LOG_NONE,
	XE_LOG_ERROR,
	XE_LOG_WARN,
	XE_LOG_INFO,
	XE_LOG_VERBOSE,
	XE_LOG_DEBUG,
	XE_LOG_TRACE
};

void xe_log_setlevel(xe_loglevel level);

void xe__print		(uint type, xe_cstr file, uint line, xe_cstr str, ...);
void xe__log		(uint type, xe_cstr name, xe_cptr addr, xe_cstr str, ...);

#define xe_print(...) 			xe__print(XE_LOG_INFO, XE_SOURCE, ##__VA_ARGS__)

#define xe_log_info(...) 		xe__log(XE_LOG_INFO, ##__VA_ARGS__)
#define xe_log_warn(...)		xe__log(XE_LOG_WARN, ##__VA_ARGS__)
#define xe_log_error(...)		xe__log(XE_LOG_ERROR, ##__VA_ARGS__)

#ifdef XE_DEBUG
	#define xe_log_verbose(...)		xe__log(XE_LOG_VERBOSE, ##__VA_ARGS__)
	#define xe_log_debug(...)		xe__log(XE_LOG_DEBUG, ##__VA_ARGS__)
	#define xe_log_trace(...)		xe__log(XE_LOG_TRACE, ##__VA_ARGS__)
#else
	#define xe_log_verbose(...)
	#define xe_log_debug(...)
	#define xe_log_trace(...)
#endif