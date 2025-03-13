#pragma once
#include "xstd/types.h"

template<typename T>
static inline uintptr_t xe_arch_alignof(T ptr, uintptr_t align){
	return (uintptr_t)ptr & (align - 1);
}

template<typename T>
static inline T xe_arch_alignto(T ptr, uintptr_t align){
	return (T)((uintptr_t)ptr & -align);
}