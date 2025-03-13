#include <string.h>
#include "../arch.h"

size_t xe_arch_strlen(xe_cptr ptr){
	return strlen((xe_cstr)ptr);
}