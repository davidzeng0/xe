#pragma once
#include "xe/debug.h"
#include "xe/types.h"
#include "xe/error.h"
#include "xe/mem.h"

template<class T>
struct xe_vector{
	T* array;

	size_t capacity;
	size_t size;

	inline T& operator[](size_t i) const{
		xe_assert(array != null);
		xe_assert(i < size);

		return array[i];
	}
};

template<class T>
static void xe_vector_cleanup(xe_vector<T>* vec){
	if(vec -> array)
		xe_dealloc(vec -> array);
}

template<class T>
static int xe_vector_copy(xe_vector<T>* dest, xe_vector<T>* src){
	if(src -> size > dest -> capacity){
		T* ptr = xe_trealloc(dest -> array, src -> size);

		if(!ptr)
			return XE_NOMEM;
		dest -> array = ptr;
		dest -> capacity = src -> size;
	}

	dest -> size = src -> size;

	xe_tmemcpy(dest -> array, src -> array, src -> size);

	return 0;
}

template<class T>
static int xe_vector_pushback(xe_vector<T>* vec, T& el){
	xe_assert(vec -> size <= vec -> capacity);
	xe_assert(vec -> size <= SIZE_MAX / sizeof(T));

	if(vec -> size == SIZE_MAX / sizeof(T))
		return XE_NOMEM;
	if(xe_vector_grow(vec, vec -> size + 1))
		return XE_NOMEM;
	vec -> array[vec -> size++] = el;

	return 0;
}

template<class T>
static int xe_vector_pushback(xe_vector<T>* vec, T&& el){
	return xe_vector_pushback(vec, el);
}

template<class T>
static T& xe_vector_popback(xe_vector<T>* vec){
	xe_assert(vec -> size > 0);

	T& result = vec -> array[vec -> size - 1];

	vec -> size--;

	return result;
}

template<class T>
static int xe_vector_resize(xe_vector<T>* vec, size_t size){
	if(size <= vec -> capacity){
		vec -> size = size;

		return 0;
	}

	T* ptr = xe_trealloc(vec -> array, size);

	if(!ptr)
		return XE_NOMEM;
	vec -> capacity = size;
	vec -> array = ptr;
	vec -> size = size;

	return 0;
}

template<class T>
static int xe_vector_grow(xe_vector<T>* vec, size_t size){
	constexpr size_t max = SIZE_MAX / sizeof(T);

	if(size > max)
		return XE_NOMEM;
	size_t cap = vec -> capacity;

	if(max >> 1 < cap)
		size = max;
	else{
		cap = cap << 1;

		if(cap > size)
			size = cap;
	}

	return xe_vector_resize(vec, size);
}

template<class T>
static void xe_vector_trim(xe_vector<T>* vec){
	if(!vec -> capacity > vec -> size)
		return;
	if(!vec -> size){
		xe_dealloc(vec -> array);

		vec -> array = null;
		vec -> capacity = 0;
	}else{
		T* ptr = xe_trealloc(vec -> array, vec -> size);

		if(!ptr)
			return;
		vec -> array = ptr;
		vec -> capacity = vec -> size;
	}
}