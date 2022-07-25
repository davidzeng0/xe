#pragma once
#include "list.h"
#include "slice.h"

template<typename T>
class xe_array : public xe_list<T>{
protected:
	T* data_;
	size_t size_;
public:
	using xe_list<T>::max_size;
	using iterator = T*;
	using const_iterator = const T*;

	constexpr xe_array(){
		data_ = null;
		size_ = 0;
	}

	constexpr xe_array(xe_array<T>&& other){
		operator=(std::move(other));
	}

	constexpr xe_array& operator=(xe_array<T>&& other){
		data_ = other.data_;
		size_ = other.size_;
		other.data_ = null;
		other.size_ = 0;

		return *this;
	}

	xe_array(const xe_array<T>& other) = delete;
	xe_array& operator=(const xe_array<T>& other) = delete;

	constexpr T& at(size_t i){
		xe_assert(data_ != null);
		xe_assert(i < size_);

		return data_[i];
	}

	constexpr const T& at(size_t i) const{
		xe_assert(data_ != null);
		xe_assert(i < size_);

		return data_[i];
	}

	constexpr T& operator[](size_t i){
		return at(i);
	}

	const T& operator[](size_t i) const{
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

	bool resize(size_t size){
		if(size > max_size())
			return false;
		T* data = xe_trealloc(data_, size);

		if(!data)
			return false;
		size_ = size;
		data_ = data;

		return true;
	}

	bool copy(const T* src_data, size_t src_size){
		if(src_size > max_size())
			return false;
		T* data = xe_alloc<T>(src_size);

		if(!data)
			return false;
		xe_dealloc(data_);

		data_ = data;
		size_ = src_size;

		xe_tmemcpy(data_, src_data, src_size);

		return true;
	}

	bool copy(const xe_array<T>& src){
		return copy(src.data_, src.size_);
	}

	void free(){
		xe_dealloc(data_);

		data_ = null;
		size_ = 0;
	}

	xe_slice<T> slice(size_t start, size_t end){
		xe_assert(start <= end);
		xe_assert(end <= size_);

		return xe_slice<T>(data_ + start, end - start);
	}

	operator xe_slice<T>(){
		return slice(0, size_);
	}

	constexpr xe_slice<const T> slice(size_t start, size_t end) const{
		xe_assert(start <= end);
		xe_assert(end <= size_);

		return xe_slice<const T>(data_ + start, end - start);
	}

	constexpr operator xe_slice<const T>() const{
		return slice(0, size_);
	}

	constexpr operator bool(){
		return data_ != null;
	}

	~xe_array(){
		free();
	}
};