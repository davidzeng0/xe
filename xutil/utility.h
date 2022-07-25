#pragma once
#include <utility>
#include "xutil/types.h"

template<class T, typename F>
constexpr uintptr_t xe_offsetof(F T::*field){
	return (uintptr_t)&(((T*)0) ->* field);
}

template<class T, typename F>
constexpr T& xe_containerof(F& of, F T::*field){
	return *(T*)((uintptr_t)&of - xe_offsetof(field));
}

template<typename T>
constexpr T xe_min(T a, T b){
	return a < b ? a : b;
}

template<typename T>
constexpr T xe_max(T a, T b){
	return a > b ? a : b;
}

template<typename T>
void xe_swap(T& a, T& b){
	T tmp = std::move(a);
	a = std::move(b);
	b = std::move(tmp);
}

#define xe_return_error(exp) 	\
	{							\
		auto err_ = exp; 		\
								\
		if(err_) return err_; 	\
	}
