#pragma once
#include <byteswap.h>
#include "xstd/types.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline ushort xe_htons(ushort value){
	return bswap_16(value);
}

static inline uint xe_htonl(uint value){
	return bswap_32(value);
}

static inline ulong xe_htonll(ulong value){
	return bswap_64(value);
}
#else
static inline ushort xe_htons(ushort value){
	return value;
}

static inline uint xe_htonl(uint value){
	return value;
}

static inline ulong xe_htonll(ulong value){
	return value;
}
#endif

static inline ushort xe_ntohs(ushort value){
	return xe_htons(value);
}

static inline uint xe_ntohl(uint value){
	return xe_htonl(value);
}

static inline ulong xe_ntohll(ulong value){
	return xe_htonll(value);
}