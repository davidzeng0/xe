#pragma once
#include "types.h"
#include "xutil/mem.h"
#include "xutil/util.h"

template<class T>
class xe_shared_data{
private:
	T data;
	ulong refs;
public:
	typedef T value_type;

	xe_shared_data(): data(), refs(){}

	template<typename = std::enable_if<std::is_move_constructible_v<T>>>
	xe_shared_data(xe_shared_data&& other): data(std::move(other.data)), refs(){}

	template<typename = std::enable_if<std::is_copy_constructible_v<T>>>
	xe_shared_data(const xe_shared_data& other): data(other.data), refs(){}

	template<typename = std::enable_if<std::is_move_constructible_v<T>>>
	xe_shared_data(T&& other): data(std::move(other)), refs(){}

	template<typename = std::enable_if<std::is_copy_constructible_v<T>>>
	xe_shared_data(const T& other): data(other), refs(){}

	operator T*(){
		return &data;
	}

	T* operator->(){
		return &data;
	}

	T& operator*(){
		return data;
	}

	void ref(){
		refs++;
	}

	void unref(){
		if(!--refs) free();
	}

	void free(){
		xe_delete(this);
	}

	~xe_shared_data() = default;
};

template<class T>
class xe_shared_ref{
private:
	xe_shared_data<T>* data;
public:
	typedef T value_type;

	xe_shared_ref(): data(){}

	xe_shared_ref(xe_shared_data<T>& data_){
		data = &data_;
		data -> ref();
	}

	xe_shared_ref(const xe_shared_ref& ref): xe_shared_ref(*ref.data){}

	xe_shared_ref(xe_shared_ref&& ref){
		data = ref.data;
		ref.data = null;
	}

	xe_shared_ref& operator=(const xe_shared_ref& ref_){
		ref(*ref_.data);

		return *this;
	}

	xe_shared_ref& operator=(xe_shared_ref&& ref){
		unref();

		data = ref.data;
		ref.data = null;

		return *this;
	}

	void ref(xe_shared_data<T>& data_){
		unref();

		data = &data_;
		data -> ref();
	}

	void unref(){
		if(!data)
			return;
		data -> unref();
		data = null;
	}

	void free(){
		unref();
	}

	operator T*(){
		return &**data;
	}

	T* operator->(){
		return &**data;
	}

	T& operator*(){
		return **data;
	}

	~xe_shared_ref(){
		free();
	}
};