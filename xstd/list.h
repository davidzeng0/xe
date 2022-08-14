#pragma once
#include "types.h"
#include "xutil/mem.h"
#include "xutil/assert.h"

template<typename T>
class xe_list{
public:
	static constexpr size_t max_size(){
		return xe_max_value<size_t>() / sizeof(T);
	}
};
