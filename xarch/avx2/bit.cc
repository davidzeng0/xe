#include "avx2.h"

uint xe_arch_ctz(uint x){
	return __tzcnt_u32(x);
}

uint xe_arch_clz(uint x){
	return __lzcnt32(x);
}

uint xe_arch_ctzl(ulong x){
	return __tzcnt_u64(x);
}

uint xe_arch_clzl(ulong x){
	return __lzcnt64(x);
}