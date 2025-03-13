#include <string.h>
#include "../arch.h"

void xe_arch_memmove(xe_ptr dest, xe_ptr src, size_t n){
	memmove(dest, src, n);
}