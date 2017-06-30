#ifndef CHECKPLUS_H
#define CHECKPLUS_H

#include <check.h>
#include <errno.h>

#ifndef HAVE_CHECK_0_11_0

#include <string.h>

#define ck_assert_ptr_null(a)      ck_assert_ptr_eq(NULL,(a))
#define ck_assert_ptr_nonnull(a)   ck_assert_ptr_ne(NULL,(a))
#define ck_assert_mem_eq(e,a,len)  ck_assert_int_eq(0, memcmp((e),(a),(len)))

#endif /* HAVE_CHECK_0_11_0 */

#define success(a) ck_assert_int_eq( 0,(a))
#define failure(a) ck_assert_int_eq(-1,(a))
#define opened(a)  ck_assert_int_ne(-1,(a))
#define error(e)   ck_assert_int_eq((e), errno)

#endif /* CHECKPLUS_H */
