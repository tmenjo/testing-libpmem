#ifndef FAST_MEMCPY_INL_H
#define FAST_MEMCPY_INL_H

#include <assert.h>
#include <emmintrin.h>
#include <stdint.h>

static inline uintptr_t not_aligned_16(const void *p)
{
	return (uintptr_t)p & (uintptr_t)15;
}

static inline size_t not_multiple_128(size_t s)
{
	return s & (size_t)127;
}

static inline void *memcpy_sse2_a128(void *dst, const void *src, size_t len)
{
	__m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
	__m128i *d = dst;
	const __m128i *s = src;
	const size_t cnt = len >> 7; /* copy 128-bytes per loop */

	for (size_t i = 0; i < cnt; ++i)
	{
		xmm0 = _mm_load_si128(s);
		xmm1 = _mm_load_si128(s + 1);
		xmm2 = _mm_load_si128(s + 2);
		xmm3 = _mm_load_si128(s + 3);
		xmm4 = _mm_load_si128(s + 4);
		xmm5 = _mm_load_si128(s + 5);
		xmm6 = _mm_load_si128(s + 6);
		xmm7 = _mm_load_si128(s + 7);
		s += 8;
		_mm_stream_si128(d,     xmm0);
		_mm_stream_si128(d + 1, xmm1);
		_mm_stream_si128(d + 2, xmm2);
		_mm_stream_si128(d + 3, xmm3);
		_mm_stream_si128(d + 4, xmm4);
		_mm_stream_si128(d + 5, xmm5);
		_mm_stream_si128(d + 6, xmm6);
		_mm_stream_si128(d + 7, xmm7);
		d += 8;
	}
	_mm_sfence();

	return dst;
}

static inline void *memcpy_chk_sse2_a128(void *dst, const void *src, size_t len)
{
	assert(!not_aligned_16(dst));
	assert(!not_aligned_16(src));
	assert(!not_multiple_128(len));

	return memcpy_sse2_a128(dst, src, len);
}

#endif
