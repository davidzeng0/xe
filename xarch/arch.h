#pragma once
#include <type_traits>
#include "cpu.h"
#include "xstd/types.h"
#include "xutil/util.h"

void xe_arch_memset(xe_ptr ptr, byte c, size_t n);
void xe_arch_memcpy(xe_ptr dest, xe_cptr src, size_t n);
void xe_arch_memmove(xe_ptr dest, xe_ptr src, size_t n);

xe_cptr xe_arch_memchr(xe_cptr ptr, byte c, size_t n);
size_t xe_arch_strlen(xe_cptr ptr);

int xe_arch_strncasecmp(xe_cptr s1, xe_cptr s2, size_t n);
int xe_arch_strncasecmpz(xe_cptr s1, xe_cptr s2, size_t n); /* guaranteed to check null chars */

int xe_arch_strncmp(xe_cptr s1, xe_cptr s2, size_t n);
int xe_arch_strncmpz(xe_cptr s1, xe_cptr s2, size_t n); /* guaranteed to check null chars */

uint xe_arch_ctz(uint x);
uint xe_arch_clz(uint x);

uint xe_arch_ctzl(ulong x);
uint xe_arch_clzl(ulong x);

static constexpr size_t xe_strlen(xe_cptr str){
	if(std::is_constant_evaluated())
		return __builtin_strlen((xe_cstr)str);
	return xe_arch_strlen(str);
}

static constexpr xe_cptr xe_memchr(xe_cptr src, byte c, size_t len){
	if(std::is_constant_evaluated())
		return __builtin_memchr(src, c, len);
	return xe_arch_memchr(src, c, len);
}

static constexpr void xe_memset(xe_ptr dest, byte c, size_t len){
	if(std::is_constant_evaluated())
		__builtin_memset(dest, c, len);
	else
		xe_arch_memset(dest, c, len);
}

static constexpr void xe_memmove(xe_ptr dest, xe_ptr src, size_t len){
	if(std::is_constant_evaluated())
		__builtin_memmove(dest, src, len);
	else
		xe_arch_memmove(dest, src, len);
}

static constexpr void xe_memcpy(xe_ptr dest, xe_cptr src, size_t len){
	if(std::is_constant_evaluated())
		__builtin_memcpy(dest, src, len);
	else
		xe_arch_memcpy(dest, src, len);
}

static constexpr int xe_strncasecmp(xe_cptr s1, xe_cptr s2, size_t n){
	if(std::is_constant_evaluated())
		return __builtin_strncasecmp((xe_cstr)s1, (xe_cstr)s2, n);
	return xe_arch_strncasecmp(s1, s2, n);
}

static constexpr int xe_strncasecmpz(xe_cptr s1, xe_cptr s2, size_t n){
	if(std::is_constant_evaluated())
		return __builtin_strncasecmp((xe_cstr)s1, (xe_cstr)s2, n);
	return xe_arch_strncasecmpz(s1, s2, n);
}

static constexpr int xe_strncmp(xe_cptr s1, xe_cptr s2, size_t n){
	if(std::is_constant_evaluated())
		return __builtin_memcmp((xe_cstr)s1, (xe_cstr)s2, n);
	return xe_arch_strncmp(s1, s2, n);
}

static constexpr int xe_strncmpz(xe_cptr s1, xe_cptr s2, size_t n){
	if(std::is_constant_evaluated())
		return __builtin_strncmp((xe_cstr)s1, (xe_cstr)s2, n);
	return xe_arch_strncmpz(s1, s2, n);
}

static constexpr uint xe_ctz(uint x){
	if(std::is_constant_evaluated())
		return __builtin_ctz(x);
	return xe_arch_ctz(x);
}

static constexpr uint xe_clz(uint x){
	if(std::is_constant_evaluated())
		return __builtin_clz(x);
	return xe_arch_clz(x);
}

static constexpr uint xe_ctzl(ulong x){
	if(std::is_constant_evaluated())
		return __builtin_ctzl(x);
	return xe_arch_ctzl(x);
}

static constexpr uint xe_clzl(ulong x){
	if(std::is_constant_evaluated())
		return __builtin_clzl(x);
	return xe_arch_clzl(x);
}