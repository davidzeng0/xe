#include "clock.h"
#include "time.h"

ulong xe_time_ns(){
	timespec spec;

	clock_gettime(CLOCK_MONOTONIC, &spec);

	return spec.tv_nsec + spec.tv_sec * (ulong)1e9;
}

ulong xe_time_ms(){
	timespec spec;

	clock_gettime(CLOCK_MONOTONIC, &spec);

	return spec.tv_nsec / XE_NANOS_PER_MS + spec.tv_sec * XE_MILLIS_PER_SEC;
}