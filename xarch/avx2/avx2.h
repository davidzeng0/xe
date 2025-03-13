#pragma once
#include <immintrin.h>
#include <stdint.h>
#include "../arch.h"
#include "../common.h"

typedef __m256i vector;
typedef __m128i vector16;

#define VECSIZE (sizeof(vector))
#define READSIZE (VECSIZE * 4)