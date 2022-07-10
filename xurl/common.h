#pragma once
#include <byteswap.h>
#include "xe/types.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint xe_htons(ushort value){
	return bswap_16(value);
}

static inline uint xe_htonl(uint value){
	return bswap_32(value);
}

static inline ulong xe_htonll(ulong value){
	return bswap_64(value);
}
#else
static inline uint xe_htons(ushort value){
	return value;
}

static inline uint xe_htonl(uint value){
	return value;
}

static inline ulong xe_htonll(ulong value){
	return value;
}
#endif