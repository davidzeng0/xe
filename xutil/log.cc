#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "log.h"
#include "assert.h"

#define XE_COLOR_RED		XE_COLOR(1)
#define XE_COLOR_GREEN		XE_COLOR(2)
#define XE_COLOR_YELLOW		XE_COLOR(3)
#define XE_COLOR_BLUE		XE_COLOR(4)
#define XE_COLOR_MAGENTA	XE_COLOR(5)
#define XE_COLOR_CYAN		XE_COLOR(6)
#define XE_COLOR(color)		"\x1b[38;5;" #color "m"
#define XE_COLOR_RESET		"\x1b[0m"

#define XE_COLOR_ERROR XE_COLOR(1)
#define XE_COLOR_WARN XE_COLOR(3)
#define XE_COLOR_INFO XE_COLOR(122)
#define XE_COLOR_VERBOSE XE_COLOR(224)
#define XE_COLOR_DEBUG XE_COLOR(14)
#define XE_COLOR_TRACE XE_COLOR(244)

#define XE_FORMAT(color) color "%-6s" XE_COLOR(252) " %12s:%-4u " XE_COLOR_RESET
#define XE_LOG_FORMAT(color) color "[%s @ %#x] " XE_COLOR_RESET

enum{
	XE_LOG_ABORT = XE_LOG_NONE
};

static xe_log_level level = XE_LOG_INFO;

void xe_log_set_level(xe_log_level level_){
	level = level_;
}

static void xe__print(uint type, xe_cstr file, uint line, xe_cstr str, va_list args){
	xe_cstr format;
	xe_cstr typestr;

	if(type > level)
		return;
	switch(type){
		case XE_LOG_ABORT:
			format = XE_FORMAT(XE_COLOR_ERROR);
			typestr = "ABORT";

			break;
		case XE_LOG_ERROR:
			format = XE_FORMAT(XE_COLOR_ERROR);
			typestr = "ERROR";

			break;
		case XE_LOG_WARN:
			format = XE_FORMAT(XE_COLOR_WARN);
			typestr = "WARN";

			break;
		case XE_LOG_INFO:
			format = XE_FORMAT(XE_COLOR_INFO);
			typestr = "INFO";

			break;
		case XE_LOG_VERBOSE:
			format = XE_FORMAT(XE_COLOR_VERBOSE);
			typestr = "VERBOSE";

			break;
		case XE_LOG_DEBUG:
			format = XE_FORMAT(XE_COLOR_DEBUG);
			typestr = "DEBUG";

			break;
		case XE_LOG_TRACE:
			format = XE_FORMAT(XE_COLOR_TRACE);
			typestr = "TRACE";

			break;
		default:
			xe_notreached();

			break;
	}

	fprintf(stderr, format, typestr, file, line);
	vfprintf(stderr, str, args);
	fprintf(stderr, "\n");
	fflush(stderr);
}

void xe__print(uint type, xe_cstr file, uint line, xe_cstr str, ...){
	va_list args;

	va_start(args, str);
	xe__print(type, file, line, str, args);
	va_end(args);
}

static void xe__log(uint type, xe_cstr name, xe_cptr addr, xe_cstr str, va_list args){
	xe_cstr format;

	if(type > level)
		return;
	switch(type){
		case XE_LOG_ABORT:
			format = XE_LOG_FORMAT(XE_COLOR_ERROR);

			break;
		case XE_LOG_ERROR:
			format = XE_LOG_FORMAT(XE_COLOR_ERROR);

			break;
		case XE_LOG_WARN:
			format = XE_LOG_FORMAT(XE_COLOR_WARN);

			break;
		case XE_LOG_INFO:
			format = XE_LOG_FORMAT(XE_COLOR_INFO);

			break;
		case XE_LOG_VERBOSE:
			format = XE_LOG_FORMAT(XE_COLOR_VERBOSE);

			break;
		case XE_LOG_DEBUG:
			format = XE_LOG_FORMAT(XE_COLOR_DEBUG);

			break;
		case XE_LOG_TRACE:
			format = XE_LOG_FORMAT(XE_COLOR_TRACE);

			break;
		default:
			xe_notreached();

			break;
	}

	fprintf(stderr, format, name, addr);
	vfprintf(stderr, str, args);
	fprintf(stderr, "\n");
	fflush(stderr);
}

void xe__log(uint type, xe_cstr name, xe_cptr addr, xe_cstr str, ...){
	va_list args;

	va_start(args, str);
	xe__log(type, name, addr, str, args);
	va_end(args);
}

void xe__assertfail(xe_cstr file, uint line, xe_cstr expr){
	xe__print(XE_LOG_ABORT, file, line, "Assertion " XE_COLOR_BLUE "%s" XE_COLOR_RESET " failed", expr);
	abort();
}

void xe__asserteqfail(xe_cstr file, uint line, intmax_t a, intmax_t b){
	xe__print(XE_LOG_ABORT, file, line, "Assertion failed, " XE_COLOR_BLUE "%zd" XE_COLOR_RESET " != " XE_COLOR_BLUE "%zd" XE_COLOR_RESET, a, b);
	abort();
}

void xe__assertfailm(xe_cstr file, uint line, xe_cstr expr, xe_cstr message){
	xe__print(XE_LOG_ABORT, file, line, "Assertion " XE_COLOR_BLUE "%s" XE_COLOR_RESET " failed: " XE_COLOR_RED "%s" XE_COLOR_RESET, expr, message);
	abort();
}

void xe__asserteqfailm(xe_cstr file, uint line, intmax_t a, intmax_t b, xe_cstr message){
	xe__print(XE_LOG_ABORT, file, line, "Assertion failed, " XE_COLOR_BLUE "%zd" XE_COLOR_RESET " != " XE_COLOR_BLUE "%zd" XE_COLOR_RESET ": " XE_COLOR_RED "%s" XE_COLOR_RESET, a, b, message);
	abort();
}

void xe__stop(xe_cstr file, uint line, xe_cstr message){
	xe__print(XE_LOG_ABORT, file, line, XE_COLOR_RED "%s" XE_COLOR_RESET, message);
	abort();
}