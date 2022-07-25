#include "../arch.h"

uint xe_arch_ctz(uint x){
	return __builtin_ctz(x);
}

uint xe_arch_clz(uint x){
	return __builtin_clz(x);
}

uint xe_arch_ctzl(ulong x){
	return __builtin_ctzl(x);
}

uint xe_arch_clzl(ulong x){
	return __builtin_clzl(x);
}