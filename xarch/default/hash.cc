#include "xconfig/cpu.h"
#include "xutil/encoding.h"
#include "xutil/endian.h"
#include "xstd/hash.h"
#include "../arch.h"
#include "../common.h"

using namespace xe_murmur_constants;

size_t xe_arch_hash_bytes(xe_cptr ptr, size_t len){
	constexpr size_t block_size = sizeof(ulong);
	const ulong* data = (const ulong*)ptr;
	const ulong* end = data + (len / block_size);
	uintptr_t align;

	ulong k, h;

	h = seed ^ (len * m);

	while(data != end){
		k = *(data++);

		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
	}

	len = xe_arch_alignof(len, block_size);

	if(!len)
		goto end;
	/* the amount of bytes we can read before crossing a page boundary */
	align = XE_PAGESIZE - xe_arch_alignof(data, XE_PAGESIZE);

	if(len <= align && block_size > align){
		/*
		 * if we can't read past the boundary and
		 * the number of bytes wanted is more than what's left,
		 * do an aligned read and bitshift after
		 */
		align = xe_arch_alignof(data, block_size);
		data = xe_arch_alignto(data, block_size);

		k = *data;

		/* trim off the bytes at the start */
		if(XE_BYTE_ORDER == XE_LITTLE_ENDIAN)
			k >>= align * 8;
		else
			k <<= align * 8;
	}else{
		k = *data;
	}

	/* trim off the bytes at the end */
	if(XE_BYTE_ORDER == XE_LITTLE_ENDIAN)
		k &= xe_max_value<ulong>() >> (8 * (block_size - len));
	else
		k &= xe_max_value<ulong>() << (8 * (block_size - len));
	h ^= k;
	h *= m;
end:
	h ^= h >> r;

	return h;
}

size_t xe_arch_hash_lowercase(xe_cptr ptr, size_t len){
	constexpr size_t block_size = sizeof(ulong);

	xe_cstr data = (xe_cstr)ptr;
	xe_cstr end = data + xe_arch_alignto(len, block_size);

	ulong k, h;

	h = seed ^ (len * m);

	while(data != end){
		k = 0;

		for(size_t i = 0; i < block_size; i++)
			k |= (ulong)xe_char_tolower(*(data++)) << (i * 8);
		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
	}

	len = xe_arch_alignof(len, block_size);

	if(!len)
		goto end;
	k = 0;

	for(size_t i = 0; i < len; i++)
		k |= (ulong)xe_char_tolower(*(data++)) << (i * 8);
	h ^= k;
	h *= m;
end:
	h ^= h >> r;

	return h;
}