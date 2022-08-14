#pragma once
#include "types.h"
#include "xutil/mem.h"

template<class T>
class xe_unique_ptr{
private:
	T* ptr;
public:
	xe_unique_ptr(): ptr(){}
	xe_unique_ptr(T* ptr) : ptr(ptr){}

	xe_unique_ptr(xe_unique_ptr&& other){
		ptr = other.ptr;
		other.ptr = null;
	}

	xe_unique_ptr& operator=(xe_unique_ptr&& other){
		free();

		ptr = other.ptr;
		other.ptr = null;

		return *this;
	}

	xe_unique_ptr(const xe_unique_ptr& src) = delete;
	xe_unique_ptr& operator=(const xe_unique_ptr& src) = delete;

	void own(T* t){
		free();

		ptr = t;
	}

	T* release(){
		T* tmp = ptr;

		ptr = null;

		return tmp;
	}

	void free(){
		xe_delete(ptr);

		ptr = null;
	}

	operator bool(){
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
		free();
	}
};