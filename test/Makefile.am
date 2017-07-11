MAINTAINERCLEANFILES = Makefile.in

AM_CFLAGS = -Wall -Wextra -Werror @CHECK_CFLAGS@
LDADD = @CHECK_LIBS@

TESTS = test_pmem test_log

check_PROGRAMS = pmem log

pmem_SOURCES = pmem.c

log_SOURCES = log.c

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
