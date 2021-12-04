#pragma once
#include <vector>
#include "../log.h"
#include "../common.h"
#include "../error.h"
#include "../mem.h"

template<typename T>
class xe_array{
protected:
	T* _data;

	size_t _size;

	bool allocate(size_t size){
		T* ptr = xe_alloc<T>(size);

		if(!ptr)
			return false;
		free();

		_data = ptr;
		_size = size;

		return true;
	}
public:
	struct iterator{
		T* ptr;

		iterator(T* _ptr){
			ptr = _ptr;
		}

		iterator(const iterator& other){
			ptr = other.ptr;
		}

		iterator& operator=(const iterator& other){
			ptr = other.ptr;

			return *this;
		}

		iterator& operator++(){
			ptr++;

			return *this;
		}

		iterator& operator--(){
			ptr--;

			return *this;
		}

		iterator operator++(int){
			return iterator(ptr++);
		}

		iterator operator--(int){
			return iterator(ptr--);
		}

		iterator& operator+=(size_t n){
			ptr += n;

			return *this;
		}

		iterator& operator-=(size_t n){
			ptr -= n;

			return *this;
		}

		iterator operator+(size_t n){
			return iterator(ptr + n);
		}

		iterator operator-(size_t n){
			return iterator(ptr - n);
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

		bool operator==(const iterator& other) const{
			return ptr == other.ptr;
		}

		bool operator!=(const iterator& other) const{
			return ptr != other.ptr;
		}
	};

	xe_array(){
		_data = null;
		_size = 0;
	}

	xe_array(T* data, size_t size){
		_data = data;
		_size = size;
	}

	xe_array(xe_array&& other){
		_data = other._data;
		_size = other._size;
		other._data = null;
		other._size = 0;
	}

	xe_array& operator=(xe_array&& other){
		_data = other._data;
		_size = other._size;
		other._data = null;
		other._size = 0;

		return *this;
	}

	xe_array(const xe_array& other){
		operator=(other);
	}

	xe_array& operator=(const xe_array& other){
		_data = other._data;
		_size = other._size;

		return *this;
	}

	T& at(size_t i){
		xe_assert(data() != null);
		xe_assert(i < size());

		return data()[i];
	}

	const T& at(size_t i) const{
		xe_assert(data() != null);
		xe_assert(i < size());

		return data()[i];
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

	T* data(){
		return _data;
	}

	const T* data() const{
		return _data;
	}

	size_t size() const{
		return _size;
	}

	bool resize(size_t size){
		if(size > max_size())
			return false;
		T* ptr = xe_trealloc(_data, size);

		if(ptr){
			_size = size;
			_data = ptr;

			return true;
		}

		return false;
	}

	bool copy(const T* src_data, size_t src_size){
		if(!allocate(src_size))
			return false;
		xe_tmemcpy(data(), src_data, src_size);

		return true;
	}

	bool copy(const xe_array<T>& src){
		return copy(src.data(), src.size());
	}

	void clear(){
		_data = null;
		_size = 0;
	}

	void free(){
		xe_dealloc(_data);
		clear();
	}

	static size_t max_size(){
		return xe_maxarraysize<T>();
	}
};


template<typename T>
class xe_vector : public xe_array<T>{
protected:
	using xe_array<T>::_data;
	using xe_array<T>::_size;
	using xe_array<T>::allocate;

	size_t _length;
public:
	using xe_array<T>::data;
	using xe_array<T>::max_size;
	using xe_array<T>::at;
	using xe_array<T>::free;

	xe_vector(){
		_length = 0;
	}

	xe_vector(xe_vector&& other){
		operator=(std::forward(other));
	}

	xe_vector& operator=(xe_vector&& other){
		_data = other.data;
		_size = other._size;
		_length = other._length;
		other._data = null;
		other._size = 0;
		other._length = 0;

		return *this;
	}

	xe_vector(const xe_vector& other){
		operator=(other);
	}

	xe_vector& operator=(const xe_vector& other){
		_data = other._data;
		_size = other._size;
		_length = other._length;

		return *this;
	}

	size_t size() const{
		return _length;
	}

	size_t capacity() const{
		return _size;
	}

	bool copy(const T* src_data, size_t src_size){
		if(src_size > capacity() && !allocate(src_size))
			return false;
		_length = src_size;

		xe_tmemcpy(data(), src_data, _length);

		return true;
	}

	bool copy(const xe_vector<T>& src){
		return copy(src.data(), src.size());
	}

	bool push_back(T& el){
		xe_assert(size() <= capacity());
		xe_assert(capacity() <= max_size());

		if(size() == max_size() || !grow(size() + 1))
			return false;
		at(_length++) = el;

		return true;
	}

	bool push_back(T&& el){
		return push_back(el);
	}

	bool append(const T* src_data, size_t src_size){
		if(size() + src_size > capacity() && !grow(size() + src_size))
			return false;
		xe_tmemcpy(data() + size(), src_data, src_size);

		_length += src_size;

		return true;
	}

	bool append(const xe_array<T>& arr){
		return append(arr.data(), arr.size());
	}

	bool append(const xe_vector<T>& arr){
		return append(arr.data(), arr.size());
	}

	T& pop_back(){
		xe_assert(size() > 0);

		return at(--_length);
	}

	bool resize(size_t size){
		if(size > capacity() && !xe_array<T>::resize(size))
			return false;
		_length = size;

		return true;
	}

	bool grow(size_t size){
		if(size > max_size())
			return false;
		size_t cap = capacity(), new_size = size;

		if(max_size() >> 1 < cap)
			new_size = max_size();
		else{
			cap = cap << 1;

			if(cap > size)
				new_size = cap;
		}

		if(!xe_array<T>::resize(new_size))
			return false;
		return true;
	}

	void trim(){
		if(capacity() <= size())
			return;
		if(size())
			xe_array<T>::resize(size());
		else
			free();
	}

	void clear(){
		_data = null;
		_length = 0;
		_size = 0;
	}

	static size_t max_capacity(){
		return max_size();
	}
};