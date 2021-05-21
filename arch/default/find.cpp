#include <string.h>
#include "arch.h"

xe_ptr xe_arch_memchr(xe_ptr ptr, int c, size_t n){
	return memchr(ptr, c, n);
}