#pragma once
#include <limits>
#include <utility>
#include "types.h"
#include "traits.h"
#include "xarch/arch.h"

template<typename T>
constexpr T xe_min(T a, T b){
	return a < b ? a : b;
}

template<typename T>
constexpr T xe_max(T a, T b){
	return a > b ? a : b;
}

template<typename T>
constexpr void xe_swap(T& a, T& b){
	T tmp = std::move(a);
	a = std::move(b);
	b = std::move(tmp);
}

template<typename T>
constexpr T xe_max_value(){
	return std::numeric_limits<T>::max();
}

template<typename T>
constexpr T xe_min_value(){
	return std::numeric_limits<T>::min();
}

template<typename T>
constexpr T xe_max_value(const T&){
	return xe_max_value<T>();
}

template<typename T>
constexpr T xe_min_value(const T&){
	return xe_min_value<T>();
}

static constexpr inline size_t xe_strlen(xe_cstr str){
	if(std::is_constant_evaluated())
		return __builtin_strlen(str);
	return xe_arch_strlen(str);
}

static constexpr inline xe_cptr xe_memchr(xe_cptr src, byte c, size_t len){
	if(std::is_constant_evaluated())
		return __builtin_memchr(src, c, len);
	return xe_arch_memchr(src, c, len);
}

static constexpr inline void xe_memset(xe_ptr dest, byte c, size_t len){
	if(std::is_constant_evaluated())
		__builtin_memset(dest, c, len);
	else
		xe_arch_memset(dest, c, len);
}

static constexpr inline void xe_memmove(xe_ptr dest, xe_ptr src, size_t len){
	if(std::is_constant_evaluated())
		__builtin_memmove(dest, src, len);
	else
		xe_arch_memmove(dest, src, len);
}

static constexpr inline void xe_memcpy(xe_ptr dest, xe_cptr src, size_t len){
	if(std::is_constant_evaluated())
		__builtin_memcpy(dest, src, len);
	else
		xe_arch_memcpy(dest, src, len);
}

static constexpr inline int xe_strncasecmp(xe_cstr s1, xe_cstr s2, size_t n){
	if(std::is_constant_evaluated())
		return __builtin_strncasecmp(s1, s2, n);
	return xe_arch_strncasecmp(s1, s2, n);
}

static constexpr inline int xe_strncasecmpz(xe_cstr s1, xe_cstr s2, size_t n){
	if(std::is_constant_evaluated())
		return __builtin_strncasecmp(s1, s2, n);
	return xe_arch_strncasecmpz(s1, s2, n);
}

static constexpr inline int xe_strncmp(xe_cstr s1, xe_cstr s2, size_t n){
	if(std::is_constant_evaluated())
		return __builtin_memcmp(s1, s2, n);
	return xe_arch_strncmp(s1, s2, n);
}

static constexpr inline int xe_strncmpz(xe_cstr s1, xe_cstr s2, size_t n){
	if(std::is_constant_evaluated())
		return __builtin_strncmp(s1, s2, n);
	return xe_arch_strncmpz(s1, s2, n);
}

static constexpr inline uint xe_ctz(uint x){
	if(std::is_constant_evaluated())
		return __builtin_ctz(x);
	return xe_arch_ctz(x);
}

static constexpr inline uint xe_clz(uint x){
	if(std::is_constant_evaluated())
		return __builtin_clz(x);
	return xe_arch_clz(x);
}

static constexpr inline uint xe_ctzl(ulong x){
	if(std::is_constant_evaluated())
		return __builtin_ctzl(x);
	return xe_arch_ctzl(x);
}

static constexpr inline uint xe_clzl(ulong x){
	if(std::is_constant_evaluated())
		return __builtin_clzl(x);
	return xe_arch_clzl(x);
}