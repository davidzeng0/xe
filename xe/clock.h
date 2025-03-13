#pragma once
#include "xstd/types.h"

enum xe_time_constants{
	XE_NANOS_PER_SEC = 1'000'000'000,
	XE_NANOS_PER_MS = 1'000'000,
	XE_NANOS_PER_US = 1'000,
	XE_MICROS_PER_SEC = 1'000'000,
	XE_MICROS_PER_MS = 1'000,
	XE_MILLIS_PER_SEC = 1'000
};

ulong xe_time_ns(); /* system time in nanoseconds */
ulong xe_time_ms(); /* system time in milliseconds */
ulong xe_realtime_ns(); /* time since epoch in nanoseconds */
ulong xe_realtime_ms(); /* time since epoch in milliseconds */