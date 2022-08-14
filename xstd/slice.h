#pragma once
#include "list.h"

template<typename T>
class xe_slice : public xe_list<T>{
protected:
	T* data_;
	size_t size_;
public:
	using xe_list<T>::max_size;
	using iterator = T*;
	using const_iterator = const T*;

	constexpr xe_slice(){
		data_ = null;
		size_ = 0;
	}

	constexpr xe_slice(T* data, size_t size){
		data_ = data;
		size_ = size;
	}

	xe_slice(xe_slice&& other){
		operator=(std::move(other));
	}

	xe_slice& operator=(xe_slice&& other){
		data_ = other.data_;
		size_ = other.size_;
		other.data_ = null;
		other.size_ = 0;

		return *this;
	}

	xe_slice(const xe_slice<T>& other) = default;
	xe_slice& operator=(const xe_slice<T>& other) = default;

	constexpr T* data(){
		return data_;
	}

	constexpr const T* data() const{
		return data_;
	}

	constexpr size_t size() const{
		return size_;
	}

	constexpr T& at(size_t i){
		xe_assert(i < size_);

		return data_[i];
	}

	constexpr const T& at(size_t i) const{
		xe_assert(i < size_);

		return data_[i];
	}

	constexpr T& operator[](size_t i){
		return at(i);
	}

	constexpr const T& operator[](size_t i) const{
		return at(i);
	}

	constexpr iterator begin(){
		return iterator(data());
	}

	constexpr iterator end(){
		return iterator(data() + size());
	}

	constexpr const_iterator begin() const{
		return const_iterator(data());
	}

	constexpr const_iterator end() const{
		return const_iterator(data() + size());
	}

	constexpr const_iterator cbegin() const{
		return const_iterator(data());
	}

	constexpr const_iterator cend() const{
		return const_iterator(data() + size());
	}

	constexpr operator bool(){
		return data_ != null;
	}

	constexpr void clear(){
		data_ = null;
		size_ = 0;
	}
};