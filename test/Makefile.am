MAINTAINERCLEANFILES = Makefile.in

AM_CFLAGS = -Wall -Wextra -Werror @CHECK_CFLAGS@
LDADD = @CHECK_LIBS@

TESTS = test_pmem test_log

check_PROGRAMS = pmem log

pmem_SOURCES = pmem.c

log_SOURCES = log.c
