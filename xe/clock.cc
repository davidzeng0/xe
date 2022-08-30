#include <time.h>
#include "clock.h"

static ulong xe_nstime(clockid_t clock){
	timespec time;

	clock_gettime(clock, &time);

	return time.tv_nsec + time.tv_sec * XE_NANOS_PER_SEC;
}

static ulong xe_mstime(clockid_t clock){
	timespec time;

	clock_gettime(clock, &time);

	return time.tv_nsec / XE_NANOS_PER_MS + time.tv_sec * XE_MILLIS_PER_SEC;
}

ulong xe_time_ns(){
	return xe_nstime(CLOCK_MONOTONIC);
}

ulong xe_time_ms(){
	return xe_mstime(CLOCK_MONOTONIC);
}

ulong xe_realtime_ns(){
	return xe_nstime(CLOCK_REALTIME);
}

ulong xe_realtime_ms(){
	return xe_mstime(CLOCK_REALTIME);
}