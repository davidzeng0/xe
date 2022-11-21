#pragma once
#include "types.h"
#include "array.h"
#include "hash.h"
#include "xutil/util.h"

class xe_string_view : public xe_slice<char>{
public:
	constexpr xe_string_view() = default;

	constexpr xe_string_view(xe_cstr string){
		operator=(string);
	}

	constexpr xe_string_view(xe_cstr string, size_t len){
		data_ = (char*)string;
		size_ = len;
	}

	constexpr xe_string_view& operator=(xe_cstr string){
		data_ = (char*)string;
		size_ = xe_strlen(string);

		return *this;
	}

	xe_string_view(xe_string_view&& other) = default;
	xe_string_view& operator=(xe_string_view&& other) = default;
	xe_string_view(const xe_string_view& src) = default;
	xe_string_view& operator=(const xe_string_view& src) = default;

	constexpr size_t length() const{
		return size_;
	}

	size_t index_of(char c) const;
	size_t index_of(char c, size_t off) const;

	bool operator==(const xe_string_view& o) const;
	bool operator==(xe_cstr o) const;

	bool equal(const xe_string_view& o) const;
	bool equal_case(const xe_string_view& o) const;

	bool equal(xe_cstr o) const;
	bool equal_case(xe_cstr o) const;

	constexpr size_t hash() const{
		return xe_hash_bytes(data_, size_);
	}

	constexpr xe_string_view substring(size_t start) const{
		return substring(start, size_);
	}

	constexpr xe_string_view substring(size_t start, size_t end) const{
		xe_assert(start <= end);
		xe_assert(end <= size_);

		return xe_string_view(data_ + start, end - start);
	}

	constexpr ~xe_string_view() = default;
};

template<>
struct xe_hash<xe_string_view>{
	size_t operator()(const xe_string_view& string) const{
		return string.hash();
	}
};

class xe_string : public xe_array<char>{
public:
	xe_string() = default;

	xe_string(xe_string&& src);
	xe_string& operator=(xe_string&& other);

	constexpr char* data(){
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

	bool operator==(const xe_string_view& o) const;
	bool operator==(xe_cstr o) const;

	bool equal(const xe_string_view& o) const;
	bool equal_case(const xe_string_view& o) const;

	bool equal(xe_cstr o) const;
	bool equal_case(xe_cstr o) const;

	bool resize(size_t size);
	bool copy(xe_cstr src, size_t n);
	bool copy(xe_cstr src);
	bool copy(const xe_slice<char>& src);

	constexpr size_t hash() const{
		return xe_hash_bytes(data_, size_);
	}

	constexpr xe_string_view substring(size_t start) const{
		return substring(start, size_);
	}

	constexpr xe_string_view substring(size_t start, size_t end) const{
		xe_assert(start <= end);
		xe_assert(end <= size_);

		return xe_string_view(data_ + start, end - start);
	}

	xe_string_view slice(size_t start, size_t end) const{
		return substring(start, end);
	}

	operator xe_string_view() const{
		return slice(0, size_);
	}

	~xe_string() = default;
};

template<>
struct xe_hash<xe_string>{
	size_t operator()(const xe_string& string) const{
		return string.hash();
	}
};