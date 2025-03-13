#include <string.h>
#include "../arch.h"

void xe_arch_memset(xe_ptr ptr, byte c, size_t n){
	memset(ptr, c, n);
}