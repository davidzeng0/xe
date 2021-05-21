#include "arch.h"

uint xe_arch_ctz(uint x){
	return __builtin_ctz(x);
}

uint xe_arch_clz(uint x){
	return __builtin_clz(x);
}

uint xe_arch_ctzl(uint x){
	return __builtin_ctzl(x);
}

uint xe_arch_clzl(uint x){
	return __builtin_clzl(x);
}