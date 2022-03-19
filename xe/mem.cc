#include <malloc.h>
#include <stdlib.h>
#include "mem.h"
#include "common.h"
#include "arch.h"

xe_ptr xe_malloc(size_t elem_size, size_t elem_count){
	ptrdiff_t total;

	if(xe_overflow_mul(total, elem_count, elem_size))
		return null;
	return malloc(total);
}

xe_ptr xe_malloc_aligned(size_t alignment, size_t elem_size, size_t elem_count){
	xe_ptr ptr;
	ptrdiff_t total;

	if(xe_overflow_mul(total, elem_count, elem_size))
		return null;
	if(!alignment)
		alignment = XE_PAGESIZE;
	if(posix_memalign(&ptr, alignment, total))
		return null;
	return ptr;
}

xe_ptr xe_calloc(size_t elem_size, size_t elem_count){
	return calloc(elem_size, elem_count);
}

xe_ptr xe_realloc(xe_ptr ptr, size_t elem_size, size_t elem_count){
	ptrdiff_t total;

	if(xe_overflow_mul(total, elem_count, elem_size))
		return null;
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