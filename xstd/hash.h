#pragma once
#include "std.h"

namespace xe_murmur_constants{
	enum xe_murmur_constants : ulong{
		m = 0xc6a4a7935bd1e995,
		r = 47,
		i = 0xff51afd7ed558ccd,
		seed = 0xe17a1465
	};
}

static inline constexpr size_t xe_hash_bytes(xe_cstr data, size_t len){
	using namespace xe_murmur_constants;

	if(!std::is_constant_evaluated())
		return xe_arch_hash_bytes(data, len);
	ulong k, h;

	h = seed ^ (len * m);

	while(len >= 8){
		k = 0;

		for(size_t i = 0; i < 8; i++)
			k |= (ulong)data[i] << (i * 8);
		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
		len -= 8;
		data += 8;
	}

	if(len){
		k = 0;

		for(size_t i = 0; i < 8; i++)
			k |= (ulong)data[i] << (i * 8);
		h ^= k;
		h *= m;
	}

	h ^= h >> r;

	return h;
}

static inline size_t xe_hash_bytes(xe_cptr data, size_t len){
	return xe_arch_hash_bytes(data, len);
}

static inline constexpr size_t xe_hash_int(ulong x){
	x ^= x >> 33;
	x *= xe_murmur_constants::i;
	x ^= x >> 33;

	return x;
}

static inline constexpr size_t xe_hash_combine(size_t a, size_t b){
	b *= xe_murmur_constants::m;
	b ^= b >> xe_murmur_constants::r;
	b *= xe_murmur_constants::m;

	a ^= b;
	a *= xe_murmur_constants::m;

	return a + 0xe6546b64;
}

template<typename T, typename E = void>
struct xe_hash{
	size_t operator()(const T* key) const{
		return xe_hash_int((ulong)key);
	}
};

template<typename T>
struct xe_hash<T, typename std::enable_if_t<std::is_integral_v<T>>>{
	size_t operator()(const T& key) const{
		return xe_hash_int((ulong)key);
	}
};