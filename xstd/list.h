#pragma once
#include "traits.h"
#include "types.h"
#include "xutil/mem.h"
#include "xutil/assert.h"


template<typename T, typename traits = xe_traits<T>>
static void xe_construct_range(T* ptr, size_t size){
	if(traits::trivially_constructible)
		return;
	for(size_t i = 0; i < size; i++)
		xe_construct(&ptr[i]);
}

template<typename T, typename traits = xe_traits<T>>
static void xe_deconstruct_range(T* ptr, size_t size){
	if(traits::trivially_destructible)
		return;
	for(size_t i = 0; i < size; i++)
		xe_deconstruct(&ptr[i]);
}

template<typename T, typename traits = xe_traits<T>>
static void xe_copy_range(T* dest, const T* src, size_t size){
	if(traits::trivially_copy_constructible)
		xe_tmemcpy(dest, src, size);
	else{
		for(size_t i = 0; i < size; i++)
			xe_construct(&dest[i], src[i]);
	}
}

template<typename T, typename traits = xe_traits<T>>
static void xe_move_range(T* dest, T* src, size_t size){
	if(traits::trivially_move_constructible)
		xe_tmemcpy(dest, src, size);
	else{
		for(size_t i = 0; i < size; i++)
			xe_construct(&dest[i], std::move(src[i]));
	}
}

template<typename T, typename traits = xe_traits<T>>
static void xe_copy_assign_range(T* dest, const T* src, size_t size){
	if(traits::trivially_copy_assignable)
		xe_tmemcpy(dest, src, size);
	else{
		for(size_t i = 0; i < size; i++)
			dest[i] = src[i];
	}
}

template<typename T, typename traits = xe_traits<T>>
static void xe_move_assign_range(T* dest, T* src, size_t size){
	if(traits::trivially_move_assignable)
		xe_tmemcpy(dest, src, size);
	else{
		for(size_t i = 0; i < size; i++)
			dest[i] = std::move(src[i]);
	}
}

template<typename T, typename traits = xe_traits<T>>
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

		xe_move_range(data, old_data, move);
		xe_construct_range(data + move, new_size - move);
		xe_deconstruct_range(old_data, old_size);
		xe_dealloc(old_data);
	}

	return data;
}

template<typename T, typename traits = xe_traits<T>>
class xe_list{
protected:
	void construct_range(T* ptr, size_t size){
		xe_construct_range(ptr, size);
	}

	void deconstruct_range(T* ptr, size_t size){
		xe_deconstruct_range(ptr, size);
	}

	void copy_range(T* dest, const T* src, size_t size){
		xe_copy_range(dest, src, size);
	}

	void move_range(T* dest, T* src, size_t size){
		xe_move_range(dest, src, size);
	}

	void copy_assign_range(T* dest, const T* src, size_t size){
		xe_copy_assign_range(dest, src, size);
	}

	void move_assign_range(T* dest, T* src, size_t size){
		xe_move_assign_range(dest, src, size);
	}

	T* realloc_range(T* old_data, size_t old_size, size_t new_size){
		return xe_realloc_range(old_data, old_size, new_size);
	}
public:
	static constexpr size_t max_size(){
		return xe_max_value<size_t>() / sizeof(T);
	}
};