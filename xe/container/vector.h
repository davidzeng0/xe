#pragma once
#include "../assert.h"
#include "../log.h"
#include "../common.h"
#include "../error.h"
#include "../mem.h"

template<class T>
struct xe_iterator{
	T* ptr;

	xe_iterator(T* ptr_){
		ptr = ptr_;
	}

	xe_iterator(const xe_iterator& other){
		ptr = other.ptr;
	}

	xe_iterator& operator=(const xe_iterator& other){
		ptr = other.ptr;

		return *this;
	}

	xe_iterator& operator++(){
		ptr++;

		return *this;
	}

	xe_iterator& operator--(){
		ptr--;

		return *this;
	}

	xe_iterator operator++(int){
		return xe_iterator(ptr++);
	}

	xe_iterator operator--(int){
		return xe_iterator(ptr--);
	}

	xe_iterator& operator+=(size_t n){
		ptr += n;

		return *this;
	}

	xe_iterator& operator-=(size_t n){
		ptr -= n;

		return *this;
	}

	xe_iterator operator+(size_t n) const{
		return xe_iterator(ptr + n);
	}

	xe_iterator operator-(size_t n) const{
		return xe_iterator(ptr - n);
	}

	T& operator[](size_t n){
		return ptr[n];
	}

	T& operator*(){
		return *ptr;
	}

	const T& operator*() const{
		return *ptr;
	}

	T* operator->(){
		return ptr;
	}

	const T* operator->() const{
		return ptr;
	}

	bool operator==(const xe_iterator& other) const{
		return ptr == other.ptr;
	}

	bool operator!=(const xe_iterator& other) const{
		return ptr != other.ptr;
	}
};

template<typename T>
class xe_array{
protected:
	T* data_;
	size_t size_;
public:
	using iterator = xe_iterator<T>;
	using const_iterator = xe_iterator<const T>;

	constexpr xe_array(){
		data_ = null;
		size_ = 0;
	}

	constexpr xe_array(T* data, size_t size){
		data_ = data;
		size_ = size;
	}

	xe_array(xe_array<T>&& other){
		operator=(std::forward<xe_array<T>>(other));
	}

	xe_array(const xe_array<T>& other){
		operator=(other);
	}

	xe_array<T>& operator=(xe_array<T>&& other){
		data_ = other.data_;
		size_ = other.size_;
		other.data_ = null;
		other.size_ = 0;

		return *this;
	}

	xe_array<T>& operator=(const xe_array<T>& other){
		data_ = other.data_;
		size_ = other.size_;

		return *this;
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

	constexpr T* data(){
		return data_;
	}

	constexpr const T* data() const{
		return data_;
	}

	constexpr size_t size() const{
		return size_;
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
		return const_iterator(data());
	}

	const_iterator cend() const{
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
		data_ = data;
		size_ = src_size;

		xe_tmemcpy(data_, src_data, src_size);

		return true;
	}

	bool copy(const xe_array<T>& src){
		return copy(src.data_, src.size_);
	}

	void clear(){
		data_ = null;
		size_ = 0;
	}

	void free(){
		xe_dealloc(data_);
		clear();
	}

	static size_t max_size(){
		return xe_maxarraysize<T>();
	}
};


template<typename T>
class xe_vector{
protected:
	T* data_;
	size_t size_;
	size_t capacity_;
public:
	using iterator = xe_iterator<T>;
	using const_iterator = xe_iterator<const T>;

	constexpr xe_vector(){
		data_ = null;
		size_ = 0;
		capacity_ = 0;
	}

	xe_vector(xe_vector<T>&& other){
		operator=(std::forward<xe_vector<T>>(other));
	}

	xe_vector(const xe_vector<T>& other){
		operator=(other);
	}

	xe_vector<T>& operator=(xe_vector<T>&& other){
		data_ = other.data_;
		capacity_ = other.capacity_;
		size_ = other._length;
		other.data_ = null;
		other.capacity_ = 0;
		other.size_ = 0;

		return *this;
	}

	xe_vector<T>& operator=(const xe_vector<T>& other){
		data_ = other.data_;
		capacity_ = other.capacity_;
		size_ = other.size_;

		return *this;
	}

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
		at(size_++) = std::move(el);

		return true;
	}

	bool append(const T* src_data, size_t src_size){
		if(src_size > capacity_ - size_ && (src_size > max_size() - size_ || !grow(size_ + src_size)))
			return false;
		xe_tmemcpy(data_ + size_, src_data, src_size);

		size_ += src_size;

		return true;
	}

	bool append(const xe_array<T>& arr){
		return append(arr.data(), arr.size());
	}

	bool append(const xe_vector<T>& arr){
		return append(arr.data_, arr.size_);
	}

	T& pop_back(){
		xe_assert(size_ > 0);

		T& rval = at(size_ - 1);

		size_--;

		return rval;
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

	bool grow(size_t size){
		if(size > max_size())
			return false;
		if(capacity_ >= size)
			return true;
		size_t capacity = capacity_, new_size = size;

		if(max_size() >> 1 < capacity)
			new_size = max_size();
		else{
			capacity = capacity << 1;

			if(capacity > size)
				new_size = capacity;
		}

		return reserve(new_size);
	}

	void trim(){
		if(capacity_ <= size_)
			return;
		if(size_)
			reserve(size_);
		else
			free();
	}

	void free(){
		xe_dealloc(data_);
		clear();
	}

	void clear(){
		data_ = null;
		size_ = 0;
		capacity_ = 0;
	}

	static size_t max_size(){
		return xe_maxarraysize<T>();
	}
};