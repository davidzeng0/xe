#include <string.h>
#include "string/lower.h"
#include "arch.h"

int xe_arch_strncasecmp(xe_ptr s1, xe_ptr s2, size_t n){
	char c1, c2;

	for(size_t off = 0; off < n; off++){
		c1 = xe_string_tolower(*((xe_cstr)s1 + off));
		c2 = xe_string_tolower(*((xe_cstr)s2 + off));

		if(c1 != c2)
			return c1 - c2;
	}

	return 0;
}

int xe_arch_strncasecmpz(xe_ptr s1, xe_ptr s2, size_t n){
	char c1, c2;

	for(size_t off = 0; off < n; off++){
		c1 = xe_string_tolower(*((xe_cstr)s1 + off));
		c2 = xe_string_tolower(*((xe_cstr)s2 + off));

		if(c1 != c2 || c1 == 0)
			return c1 - c2;
	}

	return 0;
}

int xe_arch_strncmp(xe_ptr s1, xe_ptr s2, size_t n){
	char c1, c2;

	for(size_t off = 0; off < n; off++){
		c1 = *((xe_cstr)s1 + off);
		c2 = *((xe_cstr)s2 + off);

		if(c1 != c2)
			return c1 - c2;
	}

	return 0;
}

int xe_arch_strncmpz(xe_ptr s1, xe_ptr s2, size_t n){
	char c1, c2;

	for(size_t off = 0; off < n; off++){
		c1 = *((xe_cstr)s1 + off);
		c2 = *((xe_cstr)s2 + off);

		if(c1 != c2 || c1 == 0)
			return c1 - c2;
	}

	return 0;
}