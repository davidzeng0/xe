#pragma once
#include "list.h"
#include "array.h"
#include "slice.h"

template<typename T>
class xe_vector : public xe_list<T>{
protected:
	T* data_;
	size_t size_;
	size_t capacity_;
public:
	using xe_list<T>::max_size;
	using iterator = T*;
	using const_iterator = const T*;

	constexpr xe_vector(){
		data_ = null;
		size_ = 0;
		capacity_ = 0;
	}

	constexpr xe_vector(xe_vector<T>&& other){
		operator=(std::move(other));
	}

	constexpr xe_vector& operator=(xe_vector<T>&& other){
		data_ = other.data_;
		capacity_ = other.capacity_;
		size_ = other._length;
		other.data_ = null;
		other.capacity_ = 0;
		other.size_ = 0;

		return *this;
	}

	xe_vector(const xe_vector<T>& other) = delete;
	xe_vector& operator=(const xe_vector<T>& other) = delete;

	constexpr size_t size() const{
		return size_;
	}

	constexpr size_t capacity() const{
		return capacity_;
	}

	T& at(size_t i){
		xe_assert(data_ != null);
		xe_assert(i < size_);

		return data_[i];
	}

	const T& at(size_t i) const{
		xe_assert(data_ != null);
		xe_assert(i < size_);

		return data_[i];
	}

	T& operator[](size_t i){
		return at(i);
	}

	const T& operator[](size_t i) const{
		return at(i);
	}

	iterator begin(){
		return iterator(data());
	}

	iterator end(){
		return iterator(data() + size());
	}

	const_iterator begin() const{
		return const_iterator(data());
	}

	const_iterator end() const{
		return const_iterator(data() + size());
	}

	const_iterator cbegin() const{
		return iterator(data());
	}

	const_iterator cend() const{
		return iterator(data() + size());
	}

	constexpr T* data(){
		return data_;
	}

	constexpr const T* data() const{
		return data_;
	}

	bool copy(const T* src_data, size_t src_size){
		if(src_size > max_size())
			return false;
		if(src_size > capacity_){
			T* data = xe_alloc<T>(src_size);

			if(!data)
				return false;
			free();

			data_ = data;
			capacity_ = src_size;
		}

		xe_tmemcpy(data_, src_data, src_size);

		size_ = src_size;

		return true;
	}

	bool copy(const xe_vector<T>& src){
		return copy(src.data_, src.size_);
	}

	bool push_back(T& el){
		xe_assert(size_ <= capacity_);
		xe_assert(capacity_ <= max_size());

		if(size_ >= max_size() || (size_ >= capacity_ && !grow(size_ + 1)))
			return false;
		at(size_++) = el;

		return true;
	}

	bool push_back(T&& el){
		xe_assert(size_ <= capacity_);
		xe_assert(capacity_ <= max_size());

		if(size_ >= max_size() || (size_ >= capacity_ && !grow(size_ + 1)))
			return false;
		xe_construct(&at(size_++), std::move(el));

		return true;
	}

	bool append(const T* src_data, size_t src_size){
		if(src_size > capacity_ - size_ && (src_size > max_size() - size_ || !grow(size_ + src_size)))
			return false;
		xe_tmemcpy(data_ + size_, src_data, src_size);

		size_ += src_size;

		return true;
	}

	bool append(const xe_slice<const T>& arr){
		return append(arr.data(), arr.size());
	}

	T&& pop_back(){
		xe_assert(size_ > 0);

		T rval = std::move(at(size_ - 1));

		size_--;

		return std::move(rval);
	}

	bool resize(size_t size){
		if(size > capacity_ && !reserve(size))
			return false;
		size_ = size;

		return true;
	}

	bool reserve(size_t capacity){
		xe_assert(capacity >= size_);

		if(capacity > max_size())
			return false;
		T* data = xe_trealloc(data_, capacity);

		if(!data)
			return false;
		capacity_ = capacity;
		data_ = data;

		return true;
	}

	bool grow(size_t size, size_t max){
		if(max > max_size()) max = max_size();
		if(size > max) return false;
		if(capacity_ >= size) return true;

		size_t capacity = capacity_, new_size = size;

		if(max >> 1 < capacity)
			new_size = max;
		else{
			capacity = capacity << 1;

			if(capacity > size) new_size = capacity;
		}

		return reserve(new_size);
	}

	bool grow(size_t size){
		return grow(size, max_size());
	}

	void trim(){
		if(capacity_ <= size_) return;
		if(size_) reserve(size_);
		else free();
	}

	void free(){
		xe_dealloc(data_);

		data_ = null;
		size_ = 0;
		capacity_ = 0;
	}

	xe_slice<T> slice(size_t start, size_t end){
		xe_assert(start <= end);
		xe_assert(end <= size_);

		return xe_slice<T>(data_ + start, end - start);
	}

	operator xe_slice<T>(){
		return slice(0, size_);
	}

	xe_slice<const T> slice(size_t start, size_t end) const{
		xe_assert(start <= end);
		xe_assert(end <= size_);

		return xe_slice<const T>(data_ + start, end - start);
	}

	operator xe_slice<const T>() const{
		return slice(0, size_);
	}

	constexpr operator bool(){
		return data_ != null;
	}

	~xe_vector(){
		free();
	}
};