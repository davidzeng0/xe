#pragma once
#include "std.h"
#include "xutil/mem.h"

template<typename T>
class xe_optional{
protected:
	union{
		T data;
		struct{} empty_struct;
	};
public:
	xe_optional(){}

	template<typename = std::enable_if<std::is_move_constructible_v<T>>>
	xe_optional(xe_optional&& other): data(std::move(other.data)){}

	template<typename = std::enable_if<std::is_copy_constructible_v<T>>>
	xe_optional(const xe_optional& other): data(other.data){}

	template<typename = std::enable_if<std::is_move_assignable_v<T>>>
	xe_optional& operator=(xe_optional&& other){
		data = std::move(other.data);

		return *this;
	}

	template<typename = std::enable_if<std::is_copy_assignable_v<T>>>
	xe_optional& operator=(const xe_optional& other){
		data = other.data;

		return *this;
	}

	template<typename = std::enable_if<std::is_move_constructible_v<T>>>
	xe_optional(T&& other): data(std::move(other)){}

	template<typename = std::enable_if<std::is_copy_constructible_v<T>>>
	xe_optional(const T& other): data(other){}

	template<typename = std::enable_if<std::is_move_assignable_v<T>>>
	xe_optional& operator=(T&& other){
		data = std::move(other);

		return *this;
	}

	template<typename = std::enable_if<std::is_copy_assignable_v<T>>>
	xe_optional& operator=(const T& other){
		data = other;

		return *this;
	}

	void construct(){
		xe_construct(&data);
	}

	void destruct(){
		xe_destruct(&data);
	}

	T* operator->(){
		return &data;
	}

	const T* operator->() const{
		return &data;
	}

	T& operator*(){
		return data;
	}

	const T& operator*() const{
		return data;
	}

	T& value(){
		return data;
	}

	const T& value() const{
		return data;
	}

	~xe_optional(){}
};