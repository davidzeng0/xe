#pragma once
#include <byteswap.h>
#include "xstd/types.h"

enum xe_byte_order{
	XE_LITTLE_ENDIAN,
	XE_BIG_ENDIAN
};

#if __BYTE_ORDER == __LITTLE_ENDIAN
static constexpr xe_byte_order XE_BYTE_ORDER = XE_LITTLE_ENDIAN;
#else
static constexpr xe_byte_order XE_BYTE_ORDER = XE_BIG_ENDIAN;
#endif

template<typename T>
static constexpr inline T xe_byteswap(T value);

template<>
constexpr inline ushort xe_byteswap(ushort value){
	return __builtin_bswap16(value);
}

template<>
constexpr inline uint xe_byteswap(uint value){
	return __builtin_bswap32(value);
}

template<>
constexpr inline ulong xe_byteswap(ulong value){
	return __builtin_bswap64(value);
}

template<typename T>
static constexpr inline T xe_htoe(T value, xe_byte_order endian){
	if(endian == XE_BYTE_ORDER)
		return value;
	return xe_byteswap(value);
}

template<typename T>
static constexpr inline T xe_htobe(T value){
	return xe_htoe(value, XE_BIG_ENDIAN);
}

template<typename T>
static constexpr inline T xe_htole(T value){
	return xe_htoe(value, XE_LITTLE_ENDIAN);
}

template<typename T>
static constexpr inline T xe_hton(T value){
	return xe_htobe(value);
}

static constexpr inline ushort xe_htons(ushort value){
	return xe_hton(value);
}

static constexpr inline uint xe_htonl(uint value){
	return xe_hton(value);
}

static constexpr inline ulong xe_htonll(ulong value){
	return xe_hton(value);
}

static constexpr inline ushort xe_ntohs(ushort value){
	return xe_hton(value);
}

static constexpr inline uint xe_ntohl(uint value){
	return xe_hton(value);
}

static constexpr inline ulong xe_ntohll(ulong value){
	return xe_hton(value);
}