#pragma once
#include "list.h"

template<typename T, size_t N, class traits = xe_traits<T>>
class xe_localarray : public xe_list<T, traits>{
protected:
	union{
		T data_[N];
		struct{} empty_struct;
	};

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

	constexpr xe_localarray(){
		xe_construct_range(data_, N);
	}

	constexpr xe_localarray(const xe_localarray<T, N>& src){
		xe_copy_range(data_, src.data_, N);
	}

	constexpr xe_localarray& operator=(const xe_localarray<T, N>& src){
		copy_assign_range(data_, src.data_, N);

		return *this;
	}

	constexpr xe_localarray(xe_localarray<T, N>&& src){
		xe_move_range(data_, src.data_, N);
	}

	constexpr xe_localarray& operator=(xe_localarray<T, N>&& other){
		move_assign_range(data_, other.data_, N);

		return *this;
	}

	constexpr T& at(size_t i){
		xe_assert(i < N);

		return data_[i];
	}

	constexpr const T& at(size_t i) const{
		xe_assert(i < N);

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
		return N;
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
		return at(N - 1);
	}

	constexpr const T& back() const{
		return at(N - 1);
	}

	constexpr bool empty() const{
		return N == 0;
	}

	constexpr xe_slice<T> slice(size_t start, size_t end) const{
		xe_assert(start <= end);
		xe_assert(end <= N);

		return xe_slice<T>((T*)data_ + start, end - start);
	}

	constexpr operator xe_slice<T>() const{
		return slice(0, N);
	}

	~xe_localarray(){
		deconstruct_range(data_, N);
	}
};