MAINTAINERCLEANFILES = Makefile.in

AM_CFLAGS = -Wall -Wextra -Werror @CHECK_CFLAGS@
LDADD = @CHECK_LIBS@

TESTS = test_blk test_pmem test_log test_fast_memcpy

check_PROGRAMS = blk pmem log test_fast_memcpy

blk_SOURCES = blk.c

pmem_SOURCES = pmem.c

log_SOURCES = log.c

test_fast_memcpy_SOURCES = test_fast_memcpy.c ../src/fast-memcpy-inl.h
test_fast_memcpy_CFLAGS = -I.. -I../src

EXTRA_PROGRAMS = perf
perf_SOURCES = perf.c
perf_CFLAGS = -mavx
clean-local:
	rm -f perf
perftest: perf
	@echo -----------libc----------
	@./run_perftest
	@echo ---------libpmem---------
	@./run_perftest libpmem
	@echo -----------AVX-----------
	@./run_perftest avx
