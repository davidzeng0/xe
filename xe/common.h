#pragma once
#include "types.h"

#define null nullptr

#define xe_offsetof(struct, field) ((uintptr_t)(&((struct*)0) -> field))
#define xe_containerof(struct, field, addr) ((struct*)((uintptr_t)(addr) - xe_offsetof(struct, field)))

template<typename T>
size_t xe_maxarraysize(){
	return SIZE_MAX / sizeof(T);
}

template<typename T>
T xe_min(T a, T b){
	return a < b ? a : b;
}

template<typename T>
T xe_max(T a, T b){
	return a > b ? a : b;
}