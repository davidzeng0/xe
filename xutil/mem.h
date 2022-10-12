#pragma once
#include <new>
#include "xstd/std.h"

static_assert(sizeof(ssize_t) == sizeof(ptrdiff_t));
static constexpr size_t XE_MAX_MEMORY = SSIZE_MAX;

xe_ptr xe_malloc(size_t elem_size, size_t bytes);
xe_ptr xe_malloc_aligned(size_t alignment, size_t elem_size, size_t bytes);
xe_ptr xe_calloc(size_t elem_size, size_t bytes);
xe_ptr xe_realloc(xe_ptr ptr, size_t elem_size, size_t bytes);

void xe_dealloc(xe_ptr ptr);

template<typename T>
static inline T* xe_alloc(size_t elements){
	return (T*)xe_malloc(sizeof(T), elements);
}

template<typename T>
static inline T* xe_alloc(){
	return (T*)xe_malloc(sizeof(T), 1);
}

template<typename T>
static inline T* xe_alloc_aligned(size_t align, size_t elements){
	return (T*)xe_malloc_aligned(align, sizeof(T), elements);
}

template<typename T>
static inline T* xe_zalloc(size_t elements){
	return (T*)xe_calloc(sizeof(T), elements);
}

template<typename T>
static inline T* xe_zalloc(){
	return (T*)xe_calloc(sizeof(T), 1);
}

template<typename T>
static inline T* xe_trealloc(T* ptr, size_t elements){
	return (T*)xe_realloc(ptr, sizeof(T), elements);
}

template<typename T>
static inline T* xe_trealloc(xe_ptr ptr, size_t elements){
	return (T*)xe_realloc(ptr, sizeof(T), elements);
}

template<typename T>
static inline void xe_tmemcpy(T* dest, xe_cptr src, size_t elements){
	xe_memcpy(dest, src, sizeof(T) * elements);
}

template<typename T>
static inline void xe_tmemcpy(T* dest, xe_cptr src){
	xe_memcpy(dest, src, sizeof(T));
}

template<typename T>
static inline void xe_tmemmove(T* dest, xe_ptr src, size_t elements){
	xe_memmove(dest, src, sizeof(T) * elements);
}

template<typename T>
static inline void xe_zero(T* ptr){
	xe_memset(ptr, 0, sizeof(T));
}

template<typename T>
static inline void xe_zero(T* ptr, size_t elements){
	xe_memset(ptr, 0, elements * sizeof(T));
}

template<typename... Args>
static inline void xe_zeroall(Args&& ...args){
	(xe_zero(args), ...);
}

template<class T, typename... Args>
static inline void xe_construct(T* ptr, Args&& ...args){
	new (ptr) T(std::forward<Args>(args)...);
}

template<class T>
static inline void xe_destruct(T* ptr){
	ptr -> ~T();
}

template<class T, typename... Args>
static inline T* xe_new(Args&& ...args){
	T* ptr = xe_alloc<T>();

	if(ptr)
		xe_construct(ptr, std::forward<Args>(args)...);
	return ptr;
}

template<class T, typename... Args>
static inline T* xe_znew(Args&& ...args){
	T* ptr = xe_zalloc<T>();

	if(ptr)
		xe_construct(ptr, std::forward<Args>(args)...);
	return ptr;
}

template<class T>
static inline void xe_delete(T* ptr){
	if(ptr){
		xe_destruct(ptr);
		xe_dealloc(ptr);
	}
}

template<class T>
static inline void xe_deletep(T*& ptr){
	if(ptr){
		xe_delete(ptr);

		ptr = null;
	}
}

static inline void xe_deallocp(xe_ptr& ptr){
	if(ptr){
		xe_dealloc(ptr);

		ptr = null;
	}
}