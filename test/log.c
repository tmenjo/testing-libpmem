#include "config.h" /* should be included first */

#include <check.h>
#include <errno.h>
#include <fcntl.h>
#include <libpmemlog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "checkplus.h"

#ifndef DIR_DAX
#define DIR_DAX "/mnt/pmem0/tmp"
#endif
#ifndef DIR_NONDAX
#define DIR_NONDAX "/tmp"
#endif

#define FILE_A "foo"
#define FILE_B "bar"

static PMEMlogpool *p_ = NULL, *q_ = NULL;
static void *v_ = NULL, *w_ = NULL;

static void setup_once(void)
{
	p_ = NULL;
	q_ = NULL;

	v_ = malloc(PMEMLOG_MIN_POOL);
	ck_assert_ptr_nonnull(v_);
	memset(v_, 0x00, PMEMLOG_MIN_POOL);

	w_ = malloc(PMEMLOG_MIN_POOL);
	ck_assert_ptr_nonnull(w_);
	memset(w_, 0xFF, PMEMLOG_MIN_POOL);
}

static void setup_once_daxfs(void)
{
	setup_once();
	success(chdir(DIR_DAX));
}

static void setup_once_nondaxfs(void)
{
	setup_once();
	success(chdir(DIR_NONDAX));
}

static void teardown_once(void)
{
	free(v_);
	free(w_);
}

static void setup(void)
{
	unlink(FILE_A); /* DO NOT assert */
	failure(access(FILE_A, F_OK));
	error(ENOENT);

	unlink(FILE_B); /* DO NOT assert */
	failure(access(FILE_B, F_OK));
	error(ENOENT);
}

static void teardown(void)
{
	if (p_) {
		pmemlog_close(p_);
		p_ = NULL;
	}
	if (q_) {
		pmemlog_close(q_);
		q_ = NULL;
	}
}

/*******************************************************************************
 * pmemlog_create() fails if target file already exists.
 ******************************************************************************/
START_TEST(test_create_eexist)
{
	p_ = pmemlog_create(FILE_A, PMEMLOG_MIN_POOL, 0600);
	ck_assert_ptr_nonnull(p_);
	pmemlog_close(p_);
	p_ = NULL;

	ck_assert_ptr_null(pmemlog_create(FILE_A, PMEMLOG_MIN_POOL, 0600));
	error(EEXIST);
}
END_TEST

START_TEST(test_create_eexist2)
{
	const int fd = open(FILE_A, O_WRONLY|O_CREAT|O_EXCL, 0600);
	opened(fd);
	success(close(fd));

	ck_assert_ptr_null(pmemlog_create(FILE_A, PMEMLOG_MIN_POOL, 0600));
	error(EEXIST);
}
END_TEST

/*******************************************************************************
 * pmemlog_create() and basic operations.
 ******************************************************************************/
START_TEST(test_create)
{
	p_ = pmemlog_create(FILE_A, PMEMLOG_MIN_POOL, 0600);
	ck_assert_ptr_nonnull(p_);

	/* pmemlog_nbyte() returns # bytes available for user */
	const size_t size = pmemlog_nbyte(p_);
	ck_assert_uint_eq(PMEMLOG_MIN_POOL - 8192, size);
	struct stat sb;
	success(stat(FILE_A, &sb));
	ck_assert_uint_eq(PMEMLOG_MIN_POOL, sb.st_size);

	/* pointer is set at zero first */
	ck_assert_int_eq(0, pmemlog_tell(p_));

	/* fill whole the pmemlog file */
	success(pmemlog_append(p_, v_, size));
	ck_assert_int_eq((long long)size, pmemlog_tell(p_));
	ck_assert_uint_eq(size, pmemlog_nbyte(p_));

	/* cannot append more */
	failure(pmemlog_append(p_, v_, 1));
	error(ENOSPC);
	ck_assert_int_eq((long long)size, pmemlog_tell(p_));
	ck_assert_uint_eq(size, pmemlog_nbyte(p_));

	/* pmemlog_rewind() sets the pointer to zero */
	pmemlog_rewind(p_);
	ck_assert_int_eq(0, pmemlog_tell(p_));
	ck_assert_uint_eq(size, pmemlog_nbyte(p_));
}
END_TEST

/*******************************************************************************
 * Pmemlog file has 8-byte magic string "PMEMLOG\0" in its header.
 ******************************************************************************/
START_TEST(test_header)
{
	p_ = pmemlog_create(FILE_A, PMEMLOG_MIN_POOL, 0600);
	ck_assert_ptr_nonnull(p_);
	pmemlog_close(p_);
	p_ = NULL;

	FILE *const fp = fopen(FILE_A, "r+b");
	ck_assert_ptr_nonnull(fp);

	char header[8];
	ck_assert_uint_eq(8, fread(header, sizeof(char), 8, fp));
	ck_assert_mem_eq("PMEMLOG", header, 8);

	success(fclose(fp));
}
END_TEST

/*******************************************************************************
 * pmemlog_open() fails if target file does not exists.
 ******************************************************************************/
START_TEST(test_open_enoent)
{
	ck_assert_ptr_null(pmemlog_open(FILE_A));
	error(ENOENT);
}
END_TEST

/*******************************************************************************
 * pmemlog_open() and basic operations.
 ******************************************************************************/
START_TEST(test_open)
{
	p_ = pmemlog_create(FILE_A, PMEMLOG_MIN_POOL, 0600);
	ck_assert_ptr_nonnull(p_);

	const size_t size = pmemlog_nbyte(p_);
	ck_assert_uint_lt(0, size);
	ck_assert_uint_lt(size, PMEMLOG_MIN_POOL);

	ck_assert_int_eq(0, pmemlog_tell(p_));

	/* fill half of the pmemlog file */
	const size_t half = size / 2;
	success(pmemlog_append(p_, v_, half));
	ck_assert_int_eq((long long)half, pmemlog_tell(p_));

	/* re-open pmemlog file; pointer stays */
	pmemlog_close(p_);
	p_ = pmemlog_open(FILE_A);
	ck_assert_ptr_nonnull(p_);
	ck_assert_int_eq((long long)half, pmemlog_tell(p_));

	/* pmemlog_append is atomic; if it fails, pointer stays */
	failure(pmemlog_append(p_, v_, half + 1));
	error(ENOSPC);
	ck_assert_int_eq((long long)half, pmemlog_tell(p_));

	/* rewind the pointer then re-open pmemlog file; pointer stays */
	pmemlog_rewind(p_);
	ck_assert_int_eq(0, pmemlog_tell(p_));
	pmemlog_close(p_);
	p_ = pmemlog_open(FILE_A);
	ck_assert_ptr_nonnull(p_);
	ck_assert_int_eq(0, pmemlog_tell(p_));
}
END_TEST

/*******************************************************************************
 * Pmemlog file can be unlinked while it is opened.
 ******************************************************************************/
START_TEST(test_unlink)
{
	p_ = pmemlog_create(FILE_A, PMEMLOG_MIN_POOL, 0600);
	ck_assert_ptr_nonnull(p_);

	success(access(FILE_A, F_OK));
	success(unlink(FILE_A));
	failure(access(FILE_A, F_OK));
	error(ENOENT);
}
END_TEST

/*******************************************************************************
 * Pmemlog file can be renamed while it is opened.
 ******************************************************************************/
/* callback function passed to pmemlog_walk */
int assert_walk_test_rename(const void *, size_t, void *);

START_TEST(test_rename)
{
	p_ = pmemlog_create(FILE_A, PMEMLOG_MIN_POOL, 0600);
	ck_assert_ptr_nonnull(p_);
	success(pmemlog_append(p_, v_, 8192)); /* 8192-byte 0x00 */

	q_ = pmemlog_create(FILE_B, PMEMLOG_MIN_POOL, 0600);
	ck_assert_ptr_nonnull(q_);
	success(pmemlog_append(q_, w_, 4096)); /* 4096-byte 0xFF */

	/*
         * "mv FILE_B FILE_A" while both files are opened.
	 * FILE_B should override old FILE_A then become new FILE_A.
         */
	success(rename(FILE_B, FILE_A));
	success(access(FILE_A, F_OK));
	failure(access(FILE_B, F_OK));
	error(ENOENT);

	/*
	 * Closes old FILE_A.
	 * FILE_B (that is, new FILE_A) is staying in "p_".
	 */
	PMEMlogpool *const t = p_;
	p_ = q_;
	q_ = NULL;
	pmemlog_close(t);

	/* Now "p_" should point new FILE_A. */
	ck_assert_int_eq(4096LL, pmemlog_tell(p_));
	pmemlog_walk(p_, 0, assert_walk_test_rename, NULL);
}
END_TEST

int assert_walk_test_rename(const void *buf, size_t len, void *arg)
{
	ck_assert_ptr_null(arg);
	ck_assert_uint_eq(4096, len); /* whole the content */
	ck_assert_mem_eq(w_, buf, len); /* 4096-byte 0xFF */
	return 0; /* terminate the walk */
}

START_TEST(test_rename2)
{
	p_ = pmemlog_create(FILE_A, PMEMLOG_MIN_POOL, 0600);
	ck_assert_ptr_nonnull(p_);
	success(pmemlog_append(p_, v_, 8192));

	q_ = pmemlog_create(FILE_B, PMEMLOG_MIN_POOL, 0600);
	ck_assert_ptr_nonnull(q_);
	success(pmemlog_append(q_, w_, 4096));

	/* re-open FILE_A as plain file */
	pmemlog_close(p_);
	p_ = NULL;
	const int fd = open(FILE_A, O_RDONLY);
        opened(fd);

	/*
         * "mv fuga hoge" while both files are opened.
	 * Note that FILE_A (old file) is opened as plain file
	 * while FILE_B (new file) as pmemlog file.
         */
	success(rename(FILE_B, FILE_A));
	success(access(FILE_A, F_OK));
	failure(access(FILE_B, F_OK));
	error(ENOENT);

	success(close(fd));
}
END_TEST

int main()
{
	TCase *const tcase1 = tcase_create("DAX");
	tcase_add_unchecked_fixture(tcase1, setup_once_daxfs, teardown_once);
	tcase_add_checked_fixture(tcase1, setup, teardown);
	tcase_add_test(tcase1, test_create_eexist);
	tcase_add_test(tcase1, test_create_eexist2);
	tcase_add_test(tcase1, test_create);
	tcase_add_test(tcase1, test_header);
	tcase_add_test(tcase1, test_open_enoent);
	tcase_add_test(tcase1, test_open);
	tcase_add_test(tcase1, test_unlink);
	tcase_add_test(tcase1, test_rename);
	tcase_add_test(tcase1, test_rename2);

	TCase *const tcase2 = tcase_create("non-DAX");
	tcase_add_unchecked_fixture(tcase2, setup_once_nondaxfs, teardown_once);
	tcase_add_checked_fixture(tcase2, setup, teardown);
	tcase_add_test(tcase2, test_create_eexist);
	tcase_add_test(tcase2, test_create_eexist2);
	tcase_add_test(tcase2, test_create);
	tcase_add_test(tcase2, test_header);
	tcase_add_test(tcase2, test_open_enoent);
	tcase_add_test(tcase2, test_open);
	tcase_add_test(tcase2, test_unlink);
	tcase_add_test(tcase2, test_rename);
	tcase_add_test(tcase2, test_rename2);

	Suite *const suite = suite_create("libpmemlog");
	suite_add_tcase(suite, tcase1);
	suite_add_tcase(suite, tcase2);

	SRunner *const srunner = srunner_create(suite);
	srunner_run_all(srunner, CK_NORMAL);
	const int failed = srunner_ntests_failed(srunner);
	srunner_free(srunner);

	return !!failed;
}
