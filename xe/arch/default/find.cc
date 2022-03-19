#include <string.h>
#include "../arch.h"

xe_cptr xe_arch_memchr(xe_cptr ptr, byte c, size_t n){
	return memchr(ptr, c, n);
}