#include <string.h>
#include "../arch.h"

int xe_arch_strncasecmp(xe_cptr ps1, xe_cptr ps2, size_t n){
	return strncasecmp((xe_cstr)ps1, (xe_cstr)ps2, n);
}

int xe_arch_strncasecmpz(xe_cptr ps1, xe_cptr ps2, size_t n){
	return strncasecmp((xe_cstr)ps1, (xe_cstr)ps2, n);
}

int xe_arch_strncmp(xe_cptr ps1, xe_cptr ps2, size_t n){
	return memcmp((xe_cstr)ps1, (xe_cstr)ps2, n);
}

int xe_arch_strncmpz(xe_cptr ps1, xe_cptr ps2, size_t n){
	return strncmp((xe_cstr)ps1, (xe_cstr)ps2, n);
}