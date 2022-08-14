#include <sys/random.h>
#include "random.h"
#include "xutil/mem.h"

using namespace xurl;

ssize_t xurl::xe_crypto_random(xe_ptr buf, size_t size){
	return getrandom(buf, size, 0);
}

xe_rng::xe_rng(){
	i = 0;
}

void xe_rng::seed(uint seed){
	x[0] = seed;
	i = 0;

	for(size_t i = 1; i < n; i++)
		x[i] = f * (x[i - 1] ^ (x[i - 1] >> (w - 2))) + i;
}

uint xe_rng::next(){
	size_t j, k;
	uint mask, y, z;

	j = i + 1 >= n ? 0 : i + 1;
	mask = (1 << r) - 1u;
	y = (x[i] & ~mask) | (x[j] & mask);
	k = i + m >= n ? i + m - n : i + m;
	x[i] = x[k] ^ (y >> 1) ^ (a * (y & 1));
	z = x[i] ^ ((x[i] >> u) & d);
	z ^= (z << s) & b;
	z ^= (z << t) & c;
	z ^= z >> l;
	i = j;

	return z;
}

void xe_rng::random_bytes(xe_ptr block, size_t size){
	uint* data = (uint*)block;
	uint value;

	while(size >= 4){
		*(data++) = next();
		size -= 4;
	}

	if(size){
		value = next();

		xe_memcpy(data, &value, size);
	}
}