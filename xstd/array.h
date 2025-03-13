#pragma once
#include "list.h"

template<typename T, class traits = xe_traits<T>>
class xe_array : public xe_vla<T, traits>{
protected:
	typedef xe_vla<T, traits> base;

	using base::data_;
	using base::size_;
	using base::copy_range;
	using base::realloc_range;
public:
	typedef typename base::iterator iterator;
	typedef typename base::const_iterator const_iterator;
	typedef typename base::value_type value_type;
	using base::max_size;
	using base::at;
	using base::operator[];
	using base::data;
	using base::size;
	using base::begin;
	using base::end;
	using base::cbegin;
	using base::cend;
	using base::front;
	using base::back;
	using base::slice;
	using base::empty;
	using base::operator=;
	using base::operator bool;
	using base::clear;

	xe_array() = default;
	xe_array(xe_array<T>&& other): base(std::move(other)){}

	xe_array& operator=(xe_array<T>&& other){
		base::operator=(std::move(other));

		return *this;
	}

	bool resize(size_t size){
		if(size > max_size())
			return false;
		T* data = realloc_range(data_, size_, size);

		if(!data)
			return false;
		data_ = data;
		size_ = size;

		return true;
	}

	bool copy(const T* src_data, size_t src_size){
		if(src_size > max_size())
			return false;
		T* data = xe_alloc<T>(src_size);

		if(!data)
			return false;
		clear();
		copy_range(data, src_data, src_data + src_size);

		data_ = data;
		size_ = src_size;

		return true;
	}

	bool copy(const xe_slice<T>& src){
		return copy(src.data(), src.size());
	}

	~xe_array() = default;
};