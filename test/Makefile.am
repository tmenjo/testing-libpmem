MAINTAINERCLEANFILES = Makefile.in

AM_CFLAGS = -Wall -Wextra @CHECK_CFLAGS@
LDADD = @CHECK_LIBS@

TESTS = test_libpmem \
	test_libpmemlog

check_PROGRAMS = $(TESTS)

test_libpmem_SOURCES = test_libpmem.c
test_libpmem_LDADD = -lpmem $(LDADD)

test_libpmemlog_SOURCES = test_libpmemlog.c
test_libpmemlog_LDADD = -lpmemlog -lpmem $(LDADD)
