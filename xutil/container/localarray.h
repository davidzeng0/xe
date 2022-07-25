#pragma once
#include "list.h"

template<typename T, size_t N>
class xe_localarray : public xe_list<T>{
protected:
	T data_[N];
public:
	using xe_list<T>::max_size;
	using iterator = T*;
	using const_iterator = const T*;

	constexpr xe_localarray(){}

	constexpr xe_localarray(const xe_localarray<T, N>& src){
		operator=(src);
	}

	constexpr xe_localarray& operator=(const xe_localarray<T, N>& src){
		for(size_t i = 0; i < N; i++)
			data_[i] = src.data_[i];
		return *this;
	}

	constexpr xe_localarray(xe_localarray<T, N>&& src){
		operator=(std::move(src));
	}

	constexpr xe_localarray& operator=(xe_localarray<T, N>&& other){
		for(size_t i = 0; i < N; i++)
			data_[i] = std::move(other.data_[i]);
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

	xe_slice<T> slice(size_t start, size_t end){
		xe_assert(start <= end);
		xe_assert(end <= N);

		return xe_slice<T>(data_ + start, end - start);
	}

	operator xe_slice<T>(){
		return slice(0, N);
	}

	constexpr xe_slice<const T> slice(size_t start, size_t end) const{
		xe_assert(start <= end);
		xe_assert(end <= N);

		return xe_slice<const T>(data_ + start, end - start);
	}

	constexpr operator xe_slice<const T>() const{
		return slice(0, N);
	}
};