#include <malloc.h>
#include <stdlib.h>
#include "mem.h"
#include "common.h"
#include "arch.h"

#define sizemul(a, b)							\
	size_t total;								\
												\
	if(__builtin_mul_overflow(a, b, &total))	\
		return null;

xe_ptr xe_malloc(size_t elem_size, size_t bytes){
	sizemul(elem_size, bytes)

	return malloc(total);
}

xe_ptr xe_malloc_aligned(size_t alignment, size_t elem_size, size_t bytes){
	xe_ptr ptr;

	sizemul(elem_size, bytes)

	if(!alignment)
		alignment = XE_PAGESIZE;
	if(posix_memalign(&ptr, alignment, total))
		return null;
	return ptr;
}

xe_ptr xe_calloc(size_t elem_size, size_t bytes){
	sizemul(elem_size, bytes)

	return calloc(1, total);
}

xe_ptr xe_realloc(xe_ptr ptr, size_t elem_size, size_t bytes){
	sizemul(elem_size, bytes)

	return realloc(ptr, total);
}

void xe_memset(xe_ptr ptr, byte c, size_t n){
	xe_arch_memset(ptr, c, n);
}

void xe_memcpy(xe_ptr dest, xe_cptr src, size_t n){
	xe_arch_memcpy(dest, src, n);
}

void xe_memmove(xe_ptr dest, xe_ptr src, size_t n){
	xe_arch_memmove(dest, src, n);
}

void xe_dealloc(xe_ptr ptr){
	if(ptr)
		free(ptr);
}