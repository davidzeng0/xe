#pragma once
#include "xconfig/config.h"
#include "xstd/std.h"
#include "source.h"

enum xe_log_level{
	XE_LOG_NONE,
	XE_LOG_ERROR,
	XE_LOG_WARN,
	XE_LOG_INFO,
	XE_LOG_VERBOSE,
	XE_LOG_DEBUG,
	XE_LOG_TRACE
};

void xe_log_set_level(xe_log_level level);

void xe__print(uint type, xe_cstr file, uint line, xe_cstr str, ...);
void xe__log(uint type, xe_cstr name, xe_cptr addr, xe_cstr str, ...);

#define xe_print(...)			xe__print(XE_LOG_INFO, XE_SOURCE, ##__VA_ARGS__)
#define xe_warn(...)			xe__print(XE_LOG_WARN, XE_SOURCE, ##__VA_ARGS__)
#define xe_error(...)			xe__print(XE_LOG_ERROR, XE_SOURCE, ##__VA_ARGS__)

template<class T, typename... Args>
static inline void xe_log_info(T* ptr, Args&& ...args){
	xe__log(XE_LOG_INFO, ptr -> class_name(), ptr, std::forward<Args>(args)...);
}

template<class T, typename... Args>
static inline void xe_log_warn(T* ptr, Args&& ...args){
	xe__log(XE_LOG_WARN, ptr -> class_name(), ptr, std::forward<Args>(args)...);
}

template<class T, typename... Args>
static inline void xe_log_error(T* ptr, Args&& ...args){
	xe__log(XE_LOG_ERROR, ptr -> class_name(), ptr, std::forward<Args>(args)...);
}

#ifdef XE_DEBUG
template<class T, typename... Args>
static inline void xe_log_verbose(T* ptr, Args&& ...args){
	xe__log(XE_LOG_VERBOSE, ptr -> class_name(), ptr, std::forward<Args>(args)...);
}

template<class T, typename... Args>
static inline void xe_log_debug(T* ptr, Args&& ...args){
	xe__log(XE_LOG_DEBUG, ptr -> class_name(), ptr, std::forward<Args>(args)...);
}

template<class T, typename... Args>
static inline void xe_log_trace(T* ptr, Args&& ...args){
	xe__log(XE_LOG_TRACE, ptr -> class_name(), ptr, std::forward<Args>(args)...);
}
#else
#define xe_log_verbose(...)
#define xe_log_debug(...)
#define xe_log_trace(...)
#endif