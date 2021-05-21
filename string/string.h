#pragma once
#include "xe/types.h"

size_t xe_string_length(xe_cstr str);

xe_cstr xe_string_find(xe_cstr ptr, int c, size_t n);

int xe_string_compareCase(xe_cstr s1, xe_cstr s2, size_t n);
int xe_string_compareCasez(xe_cstr s1, xe_cstr s2, size_t n);
int xe_string_compare(xe_cstr s1, xe_cstr s2, size_t n);
int xe_string_comparez(xe_cstr s1, xe_cstr s2, size_t n);

struct xe_string{
	xe_pchar data;
	size_t length;

	xe_string(){}

	xe_string(xe_cstr string){
		operator=(string);
	}

	xe_string(xe_cstr string, ulong l_len){
		data = (xe_pchar)string;
		length = l_len;
	}

	xe_string& operator=(xe_cstr string){
		data = (xe_pchar)string;
		length = xe_string_length(string);

		return *this;
	}

	inline char& operator[](ulong i_index) const{
		return data[i_index];
	}
};

int xe_string_copy(xe_string* dest, xe_cstr src, size_t n);
int xe_string_copy(xe_string* dest, xe_cstr src);
int xe_string_copy(xe_string* dest, xe_string* src);
void xe_string_cleanup(xe_string* str);

static inline int xe_string_num(char c){
	return c - '0';
}

static inline bool xe_string_isnum(char c){
	return c >= '0' && c <= '9';
}

static inline int xe_string_hex(char c){
	if(c >= '0' && c <= '9')
		return xe_string_num(c);
	if(c >= 'A' && c <= 'F')
		return 10 + c - 'A';
	if(c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	return -1;
}