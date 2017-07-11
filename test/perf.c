#include <assert.h>
#include <immintrin.h> /* AVX */
#include <xmmintrin.h> /* SSE2 (SFENCE) */
#include <libpmem.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void *memcopy_avx(void *dst, const void *src, size_t len)
{
	__m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7,
		ymm8, ymm9, ymm10, ymm11, ymm12, ymm13, ymm14, ymm15;
	__m256i *d = (__m256i *)dst;
	__m256i *s = (__m256i *)src;

	/* 512 = 32*16 = 2**9 */
	const size_t cnt = len >> 9;
	for (size_t i = 0; i < cnt; ++i) {
		/* memory -> YMM register */
		ymm0  = _mm256_loadu_si256(s);
		ymm1  = _mm256_loadu_si256(s +  1);
		ymm2  = _mm256_loadu_si256(s +  2);
		ymm3  = _mm256_loadu_si256(s +  3);
		ymm4  = _mm256_loadu_si256(s +  4);
		ymm5  = _mm256_loadu_si256(s +  5);
		ymm6  = _mm256_loadu_si256(s +  6);
		ymm7  = _mm256_loadu_si256(s +  7);
		ymm8  = _mm256_loadu_si256(s +  8);
		ymm9  = _mm256_loadu_si256(s +  9);
		ymm10 = _mm256_loadu_si256(s + 10);
		ymm11 = _mm256_loadu_si256(s + 11);
		ymm12 = _mm256_loadu_si256(s + 12);
		ymm13 = _mm256_loadu_si256(s + 13);
		ymm14 = _mm256_loadu_si256(s + 14);
		ymm15 = _mm256_loadu_si256(s + 15);
		s += 16;
		/* YMM register -> memory with VMOVNTDQ */
		_mm256_stream_si256(d,      ymm0);
		_mm256_stream_si256(d +  1, ymm1);
		_mm256_stream_si256(d +  2, ymm2);
		_mm256_stream_si256(d +  3, ymm3);
		_mm256_stream_si256(d +  4, ymm4);
		_mm256_stream_si256(d +  5, ymm5);
		_mm256_stream_si256(d +  6, ymm6);
		_mm256_stream_si256(d +  7, ymm7);
		_mm256_stream_si256(d +  8, ymm8);
		_mm256_stream_si256(d +  9, ymm9);
		_mm256_stream_si256(d + 10, ymm10);
		_mm256_stream_si256(d + 11, ymm11);
		_mm256_stream_si256(d + 12, ymm12);
		_mm256_stream_si256(d + 13, ymm13);
		_mm256_stream_si256(d + 14, ymm14);
		_mm256_stream_si256(d + 15, ymm15);
		d += 16;
	}

	return dst;
}

static void flush_nop(const void *addr, size_t len)
{
	/* do nothing */
	(void)addr;
	(void)len;
}

/* _mm_sfence() seems an inline function so this wraps it */
static void drain_sfence(void)
{
	_mm_sfence();
}

static long elapsed_us(const struct timespec *s, const struct timespec *e)
{
	return (long)(e->tv_sec - s->tv_sec) * 1000000L
		+ (e->tv_nsec - s->tv_nsec) / 1000L;
}

int main(int argc, char **argv)
{
	static const char *const TMPFILE = "/mnt/pmem0/tmp/perftest";
	static const size_t NB1G = 1 << 30;

	int r = 0;

	/* this is how memmove_nodrain_normal() does */
	void *(*fun_memcopy)(void *, const void *, size_t) = memmove;
	void (*fun_flush)(const void *, size_t) = pmem_flush;
	void (*fun_drain)(void) = pmem_drain;
	size_t alignment = sizeof(void *);

	if (argc > 1) {
		if (strcmp(argv[1], "libpmem") == 0) {
			fun_memcopy = pmem_memmove_nodrain;
			fun_flush = flush_nop;
			fun_drain = pmem_drain;
			alignment = 16;
		} else if (strcmp(argv[1], "avx") == 0) {
			fun_memcopy = memcopy_avx;
			fun_flush = flush_nop;
			fun_drain = drain_sfence;
			alignment = 32;
		}
	}

	unlink(TMPFILE);

	size_t mapped_len = 0;
	int is_pmem = 0;
	char *const dst = pmem_map_file(
		TMPFILE, NB1G,
		PMEM_FILE_CREATE|PMEM_FILE_EXCL, 0600,
		&mapped_len, &is_pmem);
	assert(dst != NULL);
	assert(((uintptr_t)dst & (alignment - 1)) == 0);
	assert(mapped_len == NB1G);
	assert(is_pmem);

	void *const src = malloc(NB1G);
	assert(src != NULL);
	memset(src, ~0, NB1G);

	struct timespec t[4] = {{0},{0},{0},{0}};
	r = clock_gettime(CLOCK_MONOTONIC, &t[0]);
	assert(r == 0);

	fun_memcopy(dst, src, NB1G);
	r = clock_gettime(CLOCK_MONOTONIC, &t[1]);
	assert(r == 0);

	fun_flush(dst, NB1G);
	r = clock_gettime(CLOCK_MONOTONIC, &t[2]);
	assert(r == 0);

	fun_drain();
	r = clock_gettime(CLOCK_MONOTONIC, &t[3]);
	assert(r == 0);

	assert(memcmp(dst, src, NB1G) == 0);

	const long memcopy_us = elapsed_us(&t[0], &t[1]);
	const long flush_us   = elapsed_us(&t[1], &t[2]);
	const long drain_us   = elapsed_us(&t[2], &t[3]);
	const long total_us   = memcopy_us + flush_us + drain_us;
	printf("%ld\t%ld\t%ld\t%ld\n",
		total_us, memcopy_us, flush_us, drain_us);

#ifdef NDEBUG
	(void)r;
#endif
	return 0;
}
