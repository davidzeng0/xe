#pragma once
#include "xe/types.h"

xe_ptr xe_malloc(size_t elem_size, size_t bytes);
xe_ptr xe_malloc_aligned(size_t alignment, size_t elem_size, size_t bytes);
xe_ptr xe_calloc(size_t elem_size, size_t bytes);
xe_ptr xe_realloc(xe_ptr ptr, size_t elem_size, size_t bytes);

void xe_memset(xe_ptr ptr, int c, size_t n);
void xe_memcpy(xe_ptr dest, xe_ptr src, size_t n);
void xe_dealloc(xe_ptr ptr);

template<class T>
static inline T* xe_alloc(size_t elements){
	return (T*)xe_malloc(sizeof(T), elements);
}

template<class T>
static inline T* xe_alloc(){
	return (T*)xe_malloc(sizeof(T), 1);
}

template<class T>
static inline T* xe_aligned_alloc(size_t align, size_t elements){
	return (T*)xe_malloc_aligned(align, sizeof(T), elements);
}

template<class T>
static inline T* xe_zalloc(size_t elements){
	return (T*)xe_calloc(sizeof(T), elements);
}

template<class T>
static inline T* xe_zalloc(){
	return (T*)xe_calloc(sizeof(T), 1);
}

template<class T>
static inline T* xe_trealloc(T* ptr, size_t elements){
	return (T*)xe_realloc(ptr, sizeof(T), elements);
}

template<class T>
static inline T* xe_trealloc(void* ptr, size_t elements){
	return (T*)xe_realloc(ptr, sizeof(T), elements);
}

template<class T>
static inline void xe_tmemcpy(T* dest, void* src, size_t elements){
	xe_memcpy(dest, src, sizeof(T) * elements);
}

template<class T>
static inline void xe_zero(T* ptr){
	xe_memset(ptr, 0, sizeof(T));
}

template<class T>
static inline void xe_zero(T* ptr, size_t elements){
	xe_memset(ptr, 0, elements * sizeof(T));
}