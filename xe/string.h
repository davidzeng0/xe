#pragma once
#include "types.h"
#include "container/vector.h"

char xe_string_tolower(char c);

size_t xe_string_length(xe_cstr str);

xe_cstr xe_string_find(xe_cstr ptr, char c, size_t n);

int xe_string_compare_case(xe_cstr s1, xe_cstr s2, size_t n);
int xe_string_compare_case_z(xe_cstr s1, xe_cstr s2, size_t n);
int xe_string_compare(xe_cstr s1, xe_cstr s2, size_t n);
int xe_string_comparez(xe_cstr s1, xe_cstr s2, size_t n);

class xe_string : public xe_array<char>{
public:
	constexpr xe_string(){}

	xe_string(xe_string&& src);
	xe_string& operator=(xe_string&& other);

	xe_string(const xe_string& other) = default;
	xe_string& operator=(const xe_string& other) = default;

	xe_string(xe_cptr string);
	xe_string(xe_cptr string, size_t len);
	xe_string& operator=(xe_cptr string);

	xe_string(xe_vector<char>&& vec);

	constexpr xe_string(xe_cstr string){
		data_ = (xe_pchar)string;
		size_ = __builtin_strlen(string);
	}

	constexpr xe_string(xe_cstr string, size_t len){
		data_ = (xe_pchar)string;
		size_ = len;
	}

	constexpr xe_string(xe_pchar string){
		data_ = string;
		size_ = __builtin_strlen(string);
	}

	constexpr xe_string(xe_pchar string, size_t len){
		data_ = string;
		size_ = len;
	}

	constexpr xe_pchar data(){
		return data_;
	}

	constexpr xe_cstr data() const{
		return data_;
	}

	constexpr xe_cstr c_str() const{
		return data_;
	}

	constexpr size_t length() const{
		return size_;
	}

	size_t index_of(char c) const;
	size_t index_of(char c, size_t off) const;

	bool operator==(const xe_string& o) const;
	bool operator==(xe_cptr o) const;

	bool equal(const xe_string& o) const;
	bool equal_case(const xe_string& o) const;

	bool equal(xe_cptr o) const;
	bool equal_case(xe_cptr o) const;

	bool copy(xe_cptr src, size_t n);
	bool copy(xe_cptr src);
	bool copy(const xe_string& src);

	void clear();

	size_t hash() const;

	xe_string substring(size_t start);
	xe_string substring(size_t start, size_t end);

	constexpr operator bool(){
		return data_ != null;
	}

	static size_t length(xe_cptr str);

	static bool equal(const xe_string& a, const xe_string& b);
	static bool equal_case(const xe_string& a, const xe_string& b);
};