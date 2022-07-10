#pragma once
#include "xe/types.h"

namespace xurl{

bool xe_crypto_random(xe_ptr buf, size_t size);

class xe_rng{
	static const uint w = 32,
		n = 624,
		m = 397,
		r = 31,
		a = 0x9908b0df,
		u = 11,
		d = 0xffffffff,
		s = 7,
		b = 0x9d2c5680,
		t = 15,
		c = 0xefc60000,
		l = 18,
		f = 1812433253;
	uint x[n];
	size_t i;
public:
	xe_rng();

	void seed(uint seed = 5489);
	void random_bytes(xe_ptr block, size_t size);
	uint next();
};

}