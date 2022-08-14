#pragma once
#include "xstd/types.h"
#include "xarch/common.h"

static inline constexpr size_t xe_hash_bytes(xe_cptr data, size_t len){
	ulong seed = 0xe17a1465,
		m = 0xc6a4a7935bd1e995,
		r = 47;
	ulong h = seed ^ (len * m);
	ulong k;

	if(std::is_constant_evaluated()){
		ulong count = 0;
		xe_cstr bytes = (xe_cstr)data;

		k = 0;

		for(size_t i = 0; i < len; i++){
			k |= (ulong)bytes[i] << (count * 8);

			if(++count >= 8){
				k *= m;
				k ^= k >> r;
				k *= m;

				h ^= k;
				h *= m;
				k = 0;
				count = 0;
			}
		}

		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
	}else{
		ulong* p = (ulong*)data;

		for(size_t i = 0; i < len >> 3; i++){
			k = *p++;

			k *= m;
			k ^= k >> r;
			k *= m;

			h ^= k;
			h *= m;
		}

		len = xe_arch_alignof(len, sizeof(ulong));

		if(len){
			k = 0;

			xe_memcpy(&k, p, len);

			k *= m;
			k ^= k >> r;
			k *= m;

			h ^= k;
			h *= m;
		}
	}

	h ^= h >> r;

	return h;
}

static inline constexpr size_t xe_hash_int(ulong x){
	x ^= x >> 33;
	x *= 0xff51afd7ed558ccd;
	x ^= x >> 33;

	return x;
}

static inline constexpr size_t xe_hash_combine(size_t h1, size_t h2){
	h1 ^= h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);

	return h1;
}

template<class T>
struct xe_hash{
	size_t operator()(const T& key) const{
		return xe_hash_int((ulong)&key);
	}
};