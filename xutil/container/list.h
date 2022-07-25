#pragma once
#include "../xutil.h"
#include "../mem.h"
#include "../assert.h"

template<typename T>
class xe_list{
public:
	static constexpr size_t max_size(){
		return SIZE_MAX / sizeof(T);
	}
};
