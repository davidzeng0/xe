#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "xe/debug.h"

#define XE_ABORT 4


#define XE_COLOR_RED		XE_COLOR(1)
#define XE_COLOR_GREEN		XE_COLOR(2)
#define XE_COLOR_YELLOW		XE_COLOR(3)
#define XE_COLOR_BLUE		XE_COLOR(4)
#define XE_COLOR_MAGENTA	XE_COLOR(5)
#define XE_COLOR_CYAN		XE_COLOR(6)
#define XE_COLOR(col)		"\x1b[38;5;" #col "m"
#define XE_COLOR_RESET		"\x1b[0m"

#define color(col) col "%-6s" XE_COLOR(252) " %12s:%-4u " XE_COLOR_RESET

void xe_debug_print(uint type, xe_cstr file, uint line, xe_cstr str, ...){
	va_list args;

	xe_cstr format;
	xe_cstr typestr;

	switch(type){
		case XE_WARN:
			format = color(XE_COLOR(208));
			typestr = "WARN";

			break;
		case XE_ERROR:
			format = color(XE_COLOR(196));
			typestr = "ERROR";

			break;
		case XE_TRACE:
			format = color(XE_COLOR(244));
			typestr = "TRACE";

			break;
		case XE_ABORT:
			format = color(XE_COLOR(196));
			typestr = "ABORT";

			break;
		case XE_INFO:
		default:
			format = color(XE_COLOR(122));
			typestr = "INFO";

			break;
	}

	printf(format, typestr, file, line);
	va_start(args, str);
	vprintf(str, args);
	va_end(args);
	printf("\n");
	fflush(stdout);
}

void xe_debug_assert(xe_cstr file, uint line, intmax_t val, xe_cstr expr){
	if(!val){
		xe_debug_print(XE_ABORT, file, line, "Assertion " XE_COLOR_BLUE "%s" XE_COLOR_RESET " failed", expr);
		abort();
	}
}

void xe_debug_asserteq(xe_cstr file, uint line, intmax_t got, intmax_t expect){
	if(got != expect){
		xe_debug_print(XE_ABORT, file, line, "Assertion failed, got " XE_COLOR_BLUE "%zd" XE_COLOR_RESET " instead of " XE_COLOR_BLUE "%zd" XE_COLOR_RESET, got, expect);
		abort();
	}
}

void xe_debug_assertm(xe_cstr file, uint line, intmax_t val, xe_cstr expr, xe_cstr message){
	if(!val){
		xe_debug_print(XE_ABORT, file, line, "Assertion " XE_COLOR_BLUE "%s" XE_COLOR_RESET " failed: " XE_COLOR_RED "%s" XE_COLOR_RESET, expr, message);
		abort();
	}
}

void xe_debug_asserteqm(xe_cstr file, uint line, intmax_t got, intmax_t expect, xe_cstr message){
	if(got != expect){
		xe_debug_print(XE_ABORT, file, line, "Assertion failed, got " XE_COLOR_BLUE "%zd" XE_COLOR_RESET " instead of " XE_COLOR_BLUE "%zd" XE_COLOR_RESET ": " XE_COLOR_RED "%s" XE_COLOR_RESET, got, expect, message);
		abort();
	}
}

void xe_debug_stop(xe_cstr file, uint line, xe_cstr message){
	xe_debug_print(XE_ABORT, file, line, XE_COLOR_RED "%s" XE_COLOR_RESET, message);
	abort();
}