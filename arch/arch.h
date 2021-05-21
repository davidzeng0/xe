#pragma once
#include "xe/types.h"
#ifdef __x86_64__
#include "x86/sys.h"
#elif __arm__
#include "arm/sys.h"
#else
#error "Unknown Architecture"
#endif

void xe_arch_memset(xe_ptr ptr, int c, size_t n);
void xe_arch_memcpy(xe_ptr dest, xe_ptr src, size_t n);

xe_ptr xe_arch_memchr(xe_ptr ptr, int c, size_t n);
size_t xe_arch_strlen(xe_ptr ptr);

int xe_arch_strncasecmp(xe_ptr s1, xe_ptr s2, size_t n); /* does not check null chars */
int xe_arch_strncasecmpz(xe_ptr s1, xe_ptr s2, size_t n); /* checks null chars */

int xe_arch_strncmp(xe_ptr s1, xe_ptr s2, size_t n); /* does not check null chars */
int xe_arch_strncmpz(xe_ptr s1, xe_ptr s2, size_t n); /* checks null chars */

uint xe_arch_ctz(uint x);
uint xe_arch_clz(uint x);

uint xe_arch_ctzl(uint x);
uint xe_arch_clzl(uint x);