#pragma once
#include "list.h"
#include "slice.h"

template<typename T, class traits = xe_traits<T>>
class xe_array : public xe_list<T, traits>{
protected:
	T* data_;
	size_t size_;

	using xe_list<T, traits>::construct_range;
	using xe_list<T, traits>::deconstruct_range;
	using xe_list<T, traits>::copy_range;
	using xe_list<T, traits>::move_range;
	using xe_list<T, traits>::realloc_range;
	using xe_list<T, traits>::copy_assign_range;
	using xe_list<T, traits>::move_assign_range;
public:
	using xe_list<T>::max_size;
	using iterator = T*;
	using const_iterator = const T*;

	constexpr xe_array(){
		data_ = null;
		size_ = 0;
	}

	constexpr xe_array(xe_array<T>&& other){
		data_ = other.data_;
		size_ = other.size_;
		other.data_ = null;
		other.size_ = 0;
	}

	constexpr xe_array& operator=(xe_array<T>&& other){
		free();

		data_ = other.data_;
		size_ = other.size_;
		other.data_ = null;
		other.size_ = 0;

		return *this;
	}

	xe_array(const xe_array<T>& other) = delete;
	xe_array& operator=(const xe_array<T>& other) = delete;

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

	constexpr T* data(){
		return data_;
	}

	constexpr const T* data() const{
		return data_;
	}

	constexpr size_t size() const{
		return size_;
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

	constexpr T& front(){
		return at(0);
	}

	constexpr const T& front() const{
		return at(0);
	}

	constexpr T& back(){
		return at(size_ - 1);
	}

	constexpr const T& back() const{
		return at(size_ - 1);
	}

	constexpr xe_slice<T> slice(size_t start, size_t end) const{
		xe_assert(start <= end);
		xe_assert(end <= size_);

		return xe_slice<T>(data_ + start, end - start);
	}

	constexpr operator xe_slice<T>() const{
		return slice(0, size_);
	}

	constexpr operator bool() const{
		return data_ != null;
	}

	constexpr bool empty() const{
		return size_ == 0;
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
		free();
		copy_range(data, src_data, src_size);

		data_ = data;
		size_ = src_size;

		return true;
	}

	bool copy(const xe_slice<T>& src){
		return copy(src.data(), src.size());
	}

	void free(){
		deconstruct_range(data_, size_);
		xe_dealloc(data_);

		data_ = null;
		size_ = 0;
	}

	~xe_array(){
		free();
	}
};