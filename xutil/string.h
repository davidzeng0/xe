#pragma once
#include "types.h"
#include "container/array.h"
#include "arch.h"
#include "hash.h"

class xe_string_view : public xe_slice<const char>{
protected:
	static bool equal(const xe_string_view& a, const xe_string_view& b);
	static bool equal_case(const xe_string_view& a, const xe_string_view& b);

	friend class xe_string;
public:
	constexpr xe_string_view(){}

	constexpr xe_string_view(xe_cptr string){
		operator=(string);
	}

	constexpr xe_string_view(xe_cptr string, size_t len){
		data_ = (xe_cstr)string;
		size_ = len;
	}

	template<typename T>
	constexpr xe_string_view(const xe_slice<T>& slice){
		data_ = (xe_cstr)slice.data();
		size_ = slice.size() * sizeof(T);
	}

	constexpr xe_string_view& operator=(xe_cptr string){
		data_ = (xe_cstr)string;
		size_ = xe_strlen(data_);

		return *this;
	}

	xe_string_view(xe_string_view&& other) = default;
	xe_string_view& operator=(xe_string_view&& other) = default;
	xe_string_view(const xe_string_view& src) = default;
	xe_string_view& operator=(const xe_string_view& src) = default;

	constexpr xe_cstr c_str() const{
		return data_;
	}

	constexpr size_t length() const{
		return size_;
	}

	size_t index_of(char c) const;
	size_t index_of(char c, size_t off) const;

	bool operator==(const xe_string_view& o) const;
	bool operator==(xe_cptr o) const;

	bool equal(const xe_string_view& o) const;
	bool equal_case(const xe_string_view& o) const;

	bool equal(xe_cptr o) const;
	bool equal_case(xe_cptr o) const;

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
};

template<>
struct xe_hash<xe_string_view>{
	size_t operator()(const xe_string_view& string) const{
		return string.hash();
	}
};

class xe_string : public xe_array<char>{
public:
	xe_string(){}

	xe_string(xe_string&& src);
	xe_string& operator=(xe_string&& other);

	xe_string(const xe_string_view& other) = delete;
	xe_string& operator=(const xe_string_view& other) = delete;

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

	bool operator==(const xe_string_view& o) const;
	bool operator==(xe_cptr o) const;

	bool equal(const xe_string_view& o) const;
	bool equal_case(const xe_string_view& o) const;

	bool equal(xe_cptr o) const;
	bool equal_case(xe_cptr o) const;

	bool copy(xe_cptr src, size_t n);
	bool copy(xe_cptr src);
	bool copy(const xe_string_view& src);

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

	~xe_string(){}
};

template<>
struct xe_hash<xe_string>{
	size_t operator()(const xe_string& string) const{
		return string.hash();
	}
};