#pragma once
#include "xstd/types.h"

#define xe_return_error(exp)	\
	do{							\
		auto err_ = (exp);		\
								\
		if(err_) [[unlikely]] return err_;	\
	}while(0)

#define xe_disable_copy_construct(class) class(const class& src) = delete;
#define xe_disable_copy_assign(class) class& operator=(const class& src) = delete;
#define xe_disable_move_construct(class) class(class&& src) = delete;
#define xe_disable_move_assign(class) class& operator=(class&& src) = delete;

#define xe_disable_copy(class)			\
	xe_disable_copy_construct(class)	\
	xe_disable_copy_assign(class)
#define xe_disable_move(class)			\
	xe_disable_move_construct(class)	\
	xe_disable_move_assign(class)
#define xe_disable_copy_move(class)	\
	xe_disable_copy(class)				\
	xe_disable_move(class)

#define xe_inline __attribute__ ((always_inline))

template<class T, typename F>
static constexpr inline uintptr_t xe_offsetof(F T::*field){
	return (uintptr_t)&(((T*)null) ->* field);
}

template<class T, typename F>
static constexpr inline T& xe_containerof(F& of, F T::*field){
	return *(T*)((uintptr_t)&of - xe_offsetof(field));
}