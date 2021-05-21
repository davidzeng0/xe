#include "string/string.h"
#include "arch.h"
#include "xe/mem.h"
#include "xe/common.h"
#include "xe/error.h"

size_t xe_string_length(xe_cstr str){
	return xe_arch_strlen((xe_ptr)str);
}

xe_cstr xe_string_find(xe_cstr ptr, int c, size_t n){
	return (xe_cstr)xe_arch_memchr((xe_ptr)ptr, c, n);
}

int xe_string_compareCase(xe_cstr s1, xe_cstr s2, size_t n){
	return xe_arch_strncasecmp((xe_ptr)s1, (xe_ptr)s2, n);
}

int xe_string_compareCasez(xe_cstr s1, xe_cstr s2, size_t n){
	return xe_arch_strncasecmpz((xe_ptr)s1, (xe_ptr)s2, n);
}

int xe_string_compare(xe_cstr s1, xe_cstr s2, size_t n){
	return xe_arch_strncmp((xe_ptr)s1, (xe_ptr)s2, n);
}

int xe_string_comparez(xe_cstr s1, xe_cstr s2, size_t n){
	return xe_arch_strncmpz((xe_ptr)s1, (xe_ptr)s2, n);
}

int xe_string_copy(xe_string* dest, xe_cstr src, size_t n){
	if(dest -> data)
		xe_dealloc(dest -> data);
	dest -> data = xe_alloc<char>(n + 1);

	if(!dest -> data)
		return XE_ENOMEM;
	xe_tmemcpy<char>(dest -> data, (xe_pchar)src, n);

	dest -> data[n] = 0;
	dest -> length = n;

	return 0;
}

int xe_string_copy(xe_string* dest, xe_cstr src){
	return xe_string_copy(dest, src, xe_string_length(src));
}

int xe_string_copy(xe_string* dest, xe_string* src){
	return xe_string_copy(dest, src -> data, src -> length);
}

void xe_string_cleanup(xe_string* str){
	xe_dealloc(str -> data);
}