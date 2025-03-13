#pragma once
#include "types.h"
#include "xutil/mem.h"
#include "xutil/util.h"

template<class T>
class xe_unique_ptr{
private:
	T* ptr;
public:
	typedef T value_type;

	xe_unique_ptr(): ptr(){}
	xe_unique_ptr(T* ptr) : ptr(ptr){}

	xe_unique_ptr(xe_unique_ptr&& other){
		ptr = other.ptr;
		other.ptr = null;
	}

	xe_unique_ptr& operator=(xe_unique_ptr&& other){
		clear();

		ptr = other.ptr;
		other.ptr = null;

		return *this;
	}

	xe_disable_copy(xe_unique_ptr)

	void own(T* t){
		clear();

		ptr = t;
	}

	T* release(){
		T* tmp = ptr;

		ptr = null;

		return tmp;
	}

	void clear(){
		xe_deletep(ptr);
	}

	operator bool() const{
		return ptr != null;
	}

	operator T*(){
		return ptr;
	}

	T* operator->(){
		return ptr;
	}

	T& operator*(){
		return *ptr;
	}

	T& operator[](size_t i){
		return ptr[i];
	}

	~xe_unique_ptr(){
		clear();
	}
};