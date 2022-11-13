#pragma once
#include "list.h"
#include "optional.h"

template<typename T, size_t N, class traits = xe_traits<T>>
class xe_fla : public xe_list<T, traits>{
protected:
	typedef xe_list<T, traits> base;

	using base::construct_range;
	using base::destruct_range;
	using base::copy_range;
	using base::move_range;
	using base::copy_assign_range;
	using base::move_assign_range;

	xe_optional<T [N]> data_;
public:
	typedef typename base::iterator iterator;
	typedef typename base::const_iterator const_iterator;
	typedef typename base::value_type value_type;
	using base::max_size;

	constexpr xe_fla(){
		construct_range(begin(), end());
	}

	constexpr xe_fla(const xe_fla<T, N>& src){
		copy_range(begin(), src.begin(), src.end());
	}

	constexpr xe_fla& operator=(const xe_fla<T, N>& src){
		copy_assign_range(begin(), src.begin(), src.end());

		return *this;
	}

	constexpr xe_fla(xe_fla<T, N>&& src){
		move_range(begin(), src.begin(), src.end());
	}

	constexpr xe_fla& operator=(xe_fla<T, N>&& other){
		move_assign_range(begin(), other.begin(), other.end());

		return *this;
	}

	constexpr T& at(size_t i){
		xe_assert(i < N);

		return (*data_)[i];
	}

	constexpr const T& at(size_t i) const{
		xe_assert(i < N);

		return (*data_)[i];
	}

	constexpr T& operator[](size_t i){
		return at(i);
	}

	constexpr const T& operator[](size_t i) const{
		return at(i);
	}

	constexpr T* data(){
		return *data_;
	}

	constexpr const T* data() const{
		return *data_;
	}

	constexpr size_t size() const{
		return N;
	}

	constexpr iterator begin(){
		return iterator(*data_);
	}

	constexpr iterator end(){
		return iterator(*data_ + N);
	}

	constexpr const_iterator begin() const{
		return const_iterator(*data_);
	}

	constexpr const_iterator end() const{
		return const_iterator(*data_ + N);
	}

	constexpr const_iterator cbegin() const{
		return const_iterator(*data_);
	}

	constexpr const_iterator cend() const{
		return const_iterator(*data_ + N);
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

	constexpr xe_slice<T, traits> slice(size_t start, size_t end) const{
		xe_assert(start <= end);
		xe_assert(end <= N);

		return xe_slice<T, traits>((T*)*data_ + start, end - start);
	}

	constexpr auto slice(size_t start){
		return slice(start, N);
	}

	constexpr auto slice(){
		return slice(0);
	}

	constexpr operator xe_slice<T, traits>(){
		return slice();
	}

	constexpr void clear(){
		destruct_range(begin(), end());
	}

	constexpr ~xe_fla(){
		clear();
	}
};