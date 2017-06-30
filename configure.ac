#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([testing-libpmem], [0.1])
AC_CONFIG_SRCDIR([test/pmem.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

PKG_CHECK_MODULES(CHECK, [check >= 0.9.4])

AH_TEMPLATE(HAVE_CHECK_0_11_0,
            [Define to 1 if you have the libcheck 0.11.0 or later.])
PKG_CHECK_MODULES(CHECK_0_11_0, [check >= 0.11.0],
                  [AC_DEFINE([HAVE_CHECK_0_11_0])], [:])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([pmem], [pmem_check_version])
AC_CHECK_LIB([pmemlog], [pmemlog_check_version])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_rdev])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_HEADER_MAJOR
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset realpath])

AC_CONFIG_FILES([Makefile
                 test/Makefile])
AC_OUTPUT
