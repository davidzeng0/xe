#pragma once
#include "std.h"
#include "xutil/mem.h"
#include "xutil/assert.h"
#include "xutil/util.h"

template<class traits, typename iterator>
static void xe_construct_range(iterator start, iterator end){
	if(traits::trivially_constructible)
		return;
	while(start != end)
		xe_construct(start++);
}

template<class traits, typename iterator>
static void xe_destruct_range(iterator start, iterator end){
	if(traits::trivially_destructible)
		return;
	while(start != end)
		xe_destruct(start++);
}

template<class traits, typename iterator, typename const_iterator>
static void xe_copy_range(iterator dest, const_iterator start, const_iterator end){
	if(traits::trivially_copy_constructible)
		xe_tmemcpy(dest, start, end - start);
	else while(start != end)
		xe_construct(dest++, *(start++));
}

template<class traits, typename iterator>
static void xe_move_range(iterator dest, iterator start, iterator end){
	if(traits::trivially_move_constructible)
		xe_tmemcpy(dest, start, end - start);
	else while(start != end)
		xe_construct(dest++, std::move(*(start++)));
}

template<class traits, typename iterator, typename const_iterator>
static void xe_copy_assign_range(iterator dest, const_iterator start, const_iterator end){
	if(traits::trivially_copy_assignable)
		xe_tmemcpy(dest, start, end - start);
	else while(start != end)
		*(dest++) = *(start++);
}

template<class traits, typename iterator>
static void xe_move_assign_range(iterator dest, iterator start, iterator end){
	if(traits::trivially_move_assignable)
		xe_tmemcpy(dest, start, end - start);
	else while(start != end)
		*(dest++) = std::move(*(start++));
}

template<typename T, class traits = xe_traits<T>>
static T* xe_realloc_range(T* old_data, size_t old_size, size_t new_size){
	T* data;
	size_t move;

	if(traits::trivially_reallocatable){
		data = xe_trealloc(old_data, new_size);

		if(!data) return null;
	}else{
		data = xe_alloc<T>(new_size);

		if(!data)
			return null;
		move = xe_min(old_size, new_size);

		xe_move_range<traits>(data, old_data, old_data + move);
		xe_construct_range<traits>(data + move, data + new_size);
		xe_destruct_range<traits>(old_data, old_data + old_size);
		xe_dealloc(old_data);
	}

	return data;
}

template<typename T, class traits = xe_traits<T>>
class xe_list{
public:
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef T value_type;

	static consteval size_t max_size(){
		return xe_max_value<size_t>() / sizeof(T);
	}
protected:
	static void construct_range(iterator start, iterator end){
		xe_construct_range<traits>(start, end);
	}

	static void destruct_range(iterator start, iterator end){
		xe_destruct_range<traits>(start, end);
	}

	static void copy_range(iterator dest, const_iterator start, const_iterator end){
		xe_copy_range<traits>(dest, start, end);
	}

	static void move_range(iterator dest, iterator start, iterator end){
		xe_move_range<traits>(dest, start, end);
	}

	static void copy_assign_range(iterator dest, const_iterator start, const_iterator end){
		xe_copy_assign_range<traits>(dest, start, end);
	}

	static void move_assign_range(iterator dest, iterator start, iterator end){
		xe_move_assign_range<traits>(dest, start, end);
	}

	static T* realloc_range(T* old_data, size_t old_size, size_t new_size){
		return xe_realloc_range<T, traits>(old_data, old_size, new_size);
	}
};

template<typename T, class traits = xe_traits<T>>
class xe_slice : public xe_list<T, traits>{
protected:
	typedef xe_list<T, traits> base;

	T* data_;
	size_t size_;

	void data_move(xe_slice<T>&& other){
		data_ = other.data_;
		size_ = other.size_;
		other.data_ = null;
		other.size_ = 0;
	}

	void data_copy(const xe_slice<T>& other){
		data_ = other.data_;
		size_ = other.size_;
	}
public:
	typedef typename base::iterator iterator;
	typedef typename base::const_iterator const_iterator;
	using base::max_size;

	constexpr xe_slice(): data_(), size_(){}

	constexpr xe_slice(T* data, size_t size){
		data_ = data;
		size_ = size;
	}

	constexpr xe_slice(const_iterator start, const_iterator end){
		data_ = start;
		size_ = end - start;
	}

	constexpr xe_slice(xe_slice&& other){
		data_move(std::move(other));
	}

	constexpr xe_slice& operator=(xe_slice&& other){
		data_move(std::move(other));

		return *this;
	}

	constexpr xe_slice(const xe_slice<T>& other){
		data_copy(other);
	}

	constexpr xe_slice& operator=(const xe_slice<T>& other){
		data_copy(other);

		return *this;
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
		return iterator(data_);
	}

	constexpr iterator end(){
		return iterator(data_ + size_);
	}

	constexpr const_iterator begin() const{
		return const_iterator(data_);
	}

	constexpr const_iterator end() const{
		return const_iterator(data_ + size_);
	}

	constexpr const_iterator cbegin() const{
		return const_iterator(data_);
	}

	constexpr const_iterator cend() const{
		return const_iterator(data_ + size_);
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

	constexpr xe_slice<T, traits> slice(size_t start, size_t end) const{
		xe_assert(start <= end);
		xe_assert(end <= size_);

		return xe_slice<T, traits>(data_ + start, end - start);
	}

	constexpr auto slice(size_t start){
		return slice(start, size_);
	}

	constexpr auto slice(){
		return slice(0);
	}

	constexpr bool empty() const{
		return size_ == 0;
	}

	constexpr operator bool() const{
		return data_ != null;
	}

	constexpr void clear(){
		data_ = null;
		size_ = 0;
	}

	~xe_slice() = default;
};

template<typename T, class traits = xe_traits<T>>
class xe_vla : public xe_slice<T, traits>{
protected:
	typedef xe_slice<T, traits> base;

	using base::data_;
	using base::size_;
	using base::data_move;
	using base::destruct_range;
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

	xe_vla() = default;
	xe_vla(xe_vla<T>&& other): base(std::move(other)){}

	xe_vla& operator=(xe_vla<T>&& other){
		clear();
		data_move(std::move(other));

		return *this;
	}

	xe_disable_copy(xe_vla)

	operator bool() const{
		return size_ > 0;
	}

	void clear(){
		destruct_range(begin(), end());
		xe_dealloc(data_);

		base::clear();
	}

	~xe_vla(){
		clear();
	}
};