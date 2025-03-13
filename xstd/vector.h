#pragma once
#include "list.h"

template<typename T, class traits = xe_traits<T>>
class xe_vector : public xe_vla<T, traits>{
protected:
	typedef xe_vla<T, traits> base;

	using base::data_;
	using base::size_;
	using base::construct_range;
	using base::copy_range;
	using base::move_range;
	using base::destruct_range;

	size_t capacity_;

	void data_move(xe_vector<T>&& other){
		base::data_move(std::move(other));

		capacity_ = other.capacity_;
		other.capacity_ = 0;
	}
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
	using base::operator bool;

	xe_vector(): base(), capacity_(){}

	xe_vector(xe_vector<T>&& other){
		data_move(std::move(other));
	}

	xe_vector& operator=(xe_vector<T>&& other){
		clear();
		data_move(std::move(other));

		return *this;
	}

	size_t capacity() const{
		return capacity_;
	}

	bool copy(const T* src_data, size_t src_size){
		if(src_size > max_size())
			return false;
		if(src_size <= capacity_)
			destruct_range(begin(), end());
		else{
			T* data = xe_alloc<T>(src_size);

			if(!data)
				return false;
			clear();

			data_ = data;
			capacity_ = src_size;
		}

		size_ = src_size;

		copy_range(data_, src_data, src_data + src_size);

		return true;
	}

	bool copy(const xe_slice<T>& src){
		return copy(src.data(), src.size());
	}

	bool push_back(const T& el){
		xe_assert(size_ <= capacity_);
		xe_assert(capacity_ <= max_size());

		if(size_ >= max_size() || (size_ >= capacity_ && !grow(size_ + 1)))
			return false;
		xe_construct(&at(size_++), el);

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
		copy_range(end(), src_data, src_data + src_size);

		size_ += src_size;

		return true;
	}

	bool append(const xe_slice<T>& arr){
		return append(arr.data(), arr.size());
	}

	T pop_back(){
		xe_assert(size_ > 0);

		T rval = std::move(at(size_ - 1));

		xe_destruct(&at(size_ - 1));

		size_--;

		return rval;
	}

	bool resize(size_t size){
		if(size < size_)
			destruct_range(data_ + size, data_ + size_);
		else{
			if(size > capacity_ && !reserve(size))
				return false;
			construct_range(data_ + size_, data_ + size);
		}

		size_ = size;

		return true;
	}

	bool reserve(size_t capacity){
		xe_assert(capacity >= size_);

		if(capacity > max_size())
			return false;
		T* data;

		if(traits::trivially_movable){
			data = xe_trealloc(data_, capacity);

			if(!data) return false;
		}else{
			data = xe_alloc<T>(capacity);

			if(!data)
				return false;
			move_range(data, begin(), end());
			destruct_range(begin(), end());
			xe_dealloc(data_);
		}

		capacity_ = capacity;
		data_ = data;

		return true;
	}

	bool grow(size_t size, size_t max){
		size_t new_size;

		if(size > max)
			return false;
		if(capacity_ >= size)
			return true;
		max = xe_min(max, max_size());
		new_size = size;

		if(max / 2 < capacity_)
			new_size = max;
		else
			new_size = xe_max(new_size, capacity_ * 2);
		return reserve(new_size);
	}

	bool grow(size_t size){
		return grow(size, max_size());
	}

	void trim(){
		if(capacity_ <= size_)
			return;
		if(size_)
			reserve(size_);
		else
			clear();
	}

	void clear(){
		base::clear();

		capacity_ = 0;
	}

	~xe_vector(){
		clear();
	}
};