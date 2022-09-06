#pragma once
#include "xstd/types.h"

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

size_t xe_arch_hash_bytes(xe_cptr data, size_t len);
size_t xe_arch_hash_lowercase(xe_cptr data, size_t len);