#include <string.h>
#include "../arch.h"

void xe_arch_memcpy(xe_ptr dest, xe_cptr src, size_t n){
	memcpy(dest, src, n);
}