#include "avx2.h"
#include "xconfig/cpu.h"

void xe_arch_memset(xe_ptr pptr, byte c, size_t n){
	byte* ptr = (byte*)pptr;

	ulong qv = (c & 0xff) * 0x0101010101010101;
	vector yv = _mm256_set1_epi64x(qv);
	vector16 xv = _mm_set1_epi64x(qv);

	uint align;

	if(n <= VECSIZE)
		goto end;
	align = xe_arch_alignof(ptr, VECSIZE);

	if(align){
		_mm256_storeu_si256((vector*)ptr, yv);

		ptr = xe_arch_alignto(ptr, VECSIZE);
		ptr += VECSIZE;
		n -= VECSIZE;
		n += align;
	}

	align = xe_arch_alignof(n, VECSIZE);
	n = xe_arch_alignto(n, VECSIZE);

	if(n > XE_CACHESIZE / 2){
		while(n){
			_mm256_stream_si256((vector*)ptr, yv);

			ptr += VECSIZE;
			n -= VECSIZE;
		}
	}else{
		while(n){
			_mm256_store_si256((vector*)ptr, yv);

			ptr += VECSIZE;
			n -= VECSIZE;
		}
	}

	if(align)
		_mm256_storeu_si256((vector*)(ptr + n - 32), yv);
	return;
end:
	if(n >= 16){
		_mm_storeu_si128((vector16*)ptr, xv);
		_mm_storeu_si128((vector16*)(ptr + n - 16), xv);
	}else if(n >= 8){
		*(ulong*)ptr = qv;
		*(ulong*)(ptr + n - 8) = qv;
	}else if(n >= 4){
		*(uint*)ptr = qv;
		*(uint*)(ptr + n - 4) = qv;
	}else if(n >= 2){
		*(ushort*)ptr = qv;
		*(ushort*)(ptr + n - 2) = qv;
	}else if(n){
		*(byte*)ptr = qv;
	}
}