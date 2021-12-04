#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "log.h"
#include "mem.h"

static int level = XE_LOG_INFO;

enum{
	XE_LOG_ABORT = XE_LOG_NONE
};

#define XE_COLOR_RED		XE_COLOR(1)
#define XE_COLOR_GREEN		XE_COLOR(2)
#define XE_COLOR_YELLOW		XE_COLOR(3)
#define XE_COLOR_BLUE		XE_COLOR(4)
#define XE_COLOR_MAGENTA	XE_COLOR(5)
#define XE_COLOR_CYAN		XE_COLOR(6)
#define XE_COLOR(color)		"\x1b[38;5;" #color "m"
#define XE_COLOR_RESET		"\x1b[0m"

#define XE_FORMAT(color) color "%-6s" XE_COLOR(252) " %12s:%-4u " XE_COLOR_RESET
#define XE_LOG_FORMAT(color) color "[%s @ %#x] " XE_COLOR_RESET

void xe_log_setlevel(xe_loglevel _level){
	level = _level;
}

static void xe__print(uint type, xe_cstr file, uint line, xe_cstr str, va_list args){
	xe_cstr format;
	xe_cstr typestr;

	if(type > level)
		return;
	switch(type){
		case XE_LOG_WARN:
			format = XE_FORMAT(XE_COLOR(208));
			typestr = "WARN";

			break;
		case XE_LOG_ERROR:
			format = XE_FORMAT(XE_COLOR(196));
			typestr = "ERROR";

			break;
		case XE_LOG_DEBUG:
			format = XE_FORMAT(XE_COLOR(223));
			typestr = "DEBUG";

			break;
		case XE_LOG_TRACE:
			format = XE_FORMAT(XE_COLOR(244));
			typestr = "TRACE";

			break;
		case XE_LOG_ABORT:
			format = XE_FORMAT(XE_COLOR(196));
			typestr = "ABORT";

			break;
		case XE_LOG_INFO:
		default:
			format = XE_FORMAT(XE_COLOR(122));
			typestr = "INFO";

			break;
	}

	printf(format, typestr, file, line);
	vprintf(str, args);
	printf("\n");
	fflush(stdout);
}

void xe__print(uint type, xe_cstr file, uint line, xe_cstr str, ...){
	va_list args;

	va_start(args, str);
	xe__print(type, file, line, str, args);
	va_end(args);
}

static void xe__log(uint type, xe_cstr name, xe_cptr addr, xe_cstr str, va_list args){
	xe_cstr format;
	xe_cstr typestr;

	if(type > level)
		return;
	switch(type){
		case XE_LOG_WARN:
			format = XE_LOG_FORMAT(XE_COLOR(208));

			break;
		case XE_LOG_ERROR:
			format = XE_LOG_FORMAT(XE_COLOR(196));

			break;
		case XE_LOG_DEBUG:
			format = XE_LOG_FORMAT(XE_COLOR(223));

			break;
		case XE_LOG_TRACE:
			format = XE_LOG_FORMAT(XE_COLOR(244));

			break;
		case XE_LOG_ABORT:
			format = XE_LOG_FORMAT(XE_COLOR(196));

			break;
		case XE_LOG_INFO:
		default:
			format = XE_LOG_FORMAT(XE_COLOR(122));

			break;
	}

	printf(format, name, addr);
	vprintf(str, args);
	printf("\n");
	fflush(stdout);
}

void xe__log(uint type, xe_cstr name, xe_cptr addr, xe_cstr str, ...){
	va_list args;

	va_start(args, str);
	xe__log(type, name, addr, str, args);
	va_end(args);
}

void xe__assert(xe_cstr file, uint line, intmax_t val, xe_cstr expr){
	if(!val){
		xe__print(XE_LOG_ABORT, file, line, "Assertion " XE_COLOR_BLUE "%s" XE_COLOR_RESET " failed", expr);
		abort();
	}
}

void xe__asserteq(xe_cstr file, uint line, intmax_t got, intmax_t expect){
	if(got != expect){
		xe__print(XE_LOG_ABORT, file, line, "Assertion failed, got " XE_COLOR_BLUE "%zd" XE_COLOR_RESET " instead of " XE_COLOR_BLUE "%zd" XE_COLOR_RESET, got, expect);
		abort();
	}
}

void xe__assertm(xe_cstr file, uint line, intmax_t val, xe_cstr expr, xe_cstr message){
	if(!val){
		xe__print(XE_LOG_ABORT, file, line, "Assertion " XE_COLOR_BLUE "%s" XE_COLOR_RESET " failed: " XE_COLOR_RED "%s" XE_COLOR_RESET, expr, message);
		abort();
	}
}

void xe__asserteqm(xe_cstr file, uint line, intmax_t got, intmax_t expect, xe_cstr message){
	if(got != expect){
		xe__print(XE_LOG_ABORT, file, line, "Assertion failed, got " XE_COLOR_BLUE "%zd" XE_COLOR_RESET " instead of " XE_COLOR_BLUE "%zd" XE_COLOR_RESET ": " XE_COLOR_RED "%s" XE_COLOR_RESET, got, expect, message);
		abort();
	}
}

void xe__stop(xe_cstr file, uint line, xe_cstr message){
	xe__print(XE_LOG_ABORT, file, line, XE_COLOR_RED "%s" XE_COLOR_RESET, message);
	abort();
}