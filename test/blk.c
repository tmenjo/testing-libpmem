#include "config.h" /* should be included first */

#include <check.h>
#include <errno.h>
#include <fcntl.h>
#include <libpmemblk.h>
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

/* global variables */
static PMEMblkpool *p_ = NULL;

/* util functions */
static PMEMblkpool *pmemblk_create_default(const char *path)
{
	return pmemblk_create(path, PMEMBLK_MIN_BLK, PMEMBLK_MIN_POOL, 0600);
}

/* fixtures */
static void setup_once_daxfs(void)
{
	success(chdir(DIR_DAX));
}

static void setup_once_nondaxfs(void)
{
	success(chdir(DIR_NONDAX));
}

static void setup(void)
{
	unlink(FILE_A); /* DO NOT assert */
	errno = 0;
	failure(access(FILE_A, F_OK));
	error(ENOENT);
}

static void teardown(void)
{
	if (p_) {
		errno = 0;
		pmemblk_close(p_);
		error(0);
		p_ = NULL;
	}
}

/* test cases */
START_TEST(create_OK)
{
	p_ = pmemblk_create_default(FILE_A);
	ck_assert_ptr_nonnull(p_);

	const size_t bsize = pmemblk_bsize(p_);
	ck_assert_uint_eq(PMEMBLK_MIN_BLK, bsize);

	const size_t nblock = pmemblk_nblock(p_);
	/*
	 * "Given the specifics of the implementation, the number of
	 * available blocks for the user cannot be less than 256."
	 * <http://pmem.io/nvml/manpages/linux/master/libpmemblk.3.html>
	 */
	ck_assert_uint_le(256, nblock);
	/*
	 * "Since the transactional nature of a block memory pool requires
	 * some space overhead in the memory pool, the resulting number of
	 * available blocks is less than poolsize/bsize."
	 * <http://pmem.io/nvml/manpages/linux/master/libpmemblk.3.html>
	 */
	ck_assert_uint_gt(PMEMBLK_MIN_POOL, bsize * nblock);

	struct stat st;
	success(stat(FILE_A, &st));
	ck_assert_uint_eq(PMEMBLK_MIN_POOL, st.st_size);

	char zbuf[PMEMBLK_MIN_BLK], obuf[PMEMBLK_MIN_BLK];
	memset(zbuf, 0x00, sizeof(zbuf));
	memset(obuf, 0xFF, sizeof(obuf));

	/* read from the block 0 which filled with zeroes initially */
	char buf[PMEMBLK_MIN_BLK];
	memset(buf, 0xFF, sizeof(buf));
	success(pmemblk_read(p_, buf, 0LL));
	ck_assert_mem_eq(zbuf, buf, PMEMBLK_MIN_BLK);

	/* write ones to the first block */
	success(pmemblk_write(p_, obuf, 0LL));
	success(pmemblk_read(p_, buf, 0LL));
	ck_assert_mem_eq(obuf, buf, PMEMBLK_MIN_BLK);

	/* fill the first block with zeroes */
	success(pmemblk_set_zero(p_, 0LL));
	success(pmemblk_read(p_, buf, 0LL));
	ck_assert_mem_eq(zbuf, buf, PMEMBLK_MIN_BLK);

	/* set the error flag to the last block */
	const long long last = (long long)nblock - 1LL;
	success(pmemblk_set_error(p_, last));
	errno = 0;
	failure(pmemblk_read(p_, buf, last));
	error(EIO);

	/* write to the last block to clear its error flag */
	success(pmemblk_write(p_, obuf, last));
	success(pmemblk_read(p_, buf, last));
	ck_assert_mem_eq(obuf, buf, PMEMBLK_MIN_BLK);

	/* every function above changes neither bsize nor nblock */
	ck_assert_uint_eq(bsize, pmemblk_bsize(p_));
	ck_assert_uint_eq(nblock, pmemblk_nblock(p_));
}
END_TEST

START_TEST(create_OK_bsize_8KiB)
{
	static const size_t NB8K = 1 << 13;

	p_ = pmemblk_create(FILE_A, NB8K, PMEMBLK_MIN_POOL, 0600);
	ck_assert_ptr_nonnull(p_);

	const size_t bsize = pmemblk_bsize(p_);
	ck_assert_uint_eq(NB8K, bsize);

	const size_t nblock = pmemblk_nblock(p_);
	ck_assert_uint_le(256, nblock);
	ck_assert_uint_gt(PMEMBLK_MIN_POOL, bsize * nblock);
}
END_TEST

#define assert_create_EINVAL_bsize(bsize_) do { \
	errno = 0; \
	ck_assert_ptr_null(pmemblk_create( \
		FILE_A, (bsize_), PMEMBLK_MIN_POOL, 0600)); \
	error(EINVAL); \
} while(0)

START_TEST(create_EINVAL_bsize)
{
	/* invalid range (out of positive uint32_t) */
	assert_create_EINVAL_bsize(0);
	assert_create_EINVAL_bsize((size_t)UINT32_MAX + 1);

	/* larger than poolsize */
	assert_create_EINVAL_bsize((size_t)PMEMBLK_MIN_POOL + 1);
	assert_create_EINVAL_bsize((size_t)PMEMBLK_MIN_POOL * 2);
	assert_create_EINVAL_bsize(UINT32_MAX);
}
END_TEST

START_TEST(create_EEXIST_pmemblk)
{
	p_ = pmemblk_create_default(FILE_A);
	ck_assert_ptr_nonnull(p_);
	pmemblk_close(p_);
	p_ = NULL;

	errno = 0;
	ck_assert_ptr_null(pmemblk_create_default(FILE_A));
	error(EEXIST);
}
END_TEST

START_TEST(create_EEXIST_fd)
{
	const int fd = open(FILE_A, O_WRONLY|O_CREAT|O_EXCL, 0600);
	opened(fd);
	success(close(fd));

	errno = 0;
	ck_assert_ptr_null(pmemblk_create_default(FILE_A));
	error(EEXIST);
}
END_TEST

START_TEST(header_PMEMBLK)
{
	p_ = pmemblk_create_default(FILE_A);
	ck_assert_ptr_nonnull(p_);
	pmemblk_close(p_);
	p_ = NULL;

	FILE *const fp = fopen(FILE_A, "r+b");
	ck_assert_ptr_nonnull(fp);

	char header[8];
	ck_assert_uint_eq(8, fread(header, sizeof(char), 8, fp));
	ck_assert_mem_eq("PMEMBLK", header, 8);

	success(fclose(fp));
}
END_TEST

START_TEST(readwrite_EINVAL)
{
	p_ = pmemblk_create_default(FILE_A);
	ck_assert_ptr_nonnull(p_);

	const size_t bsize = pmemblk_bsize(p_);
	ck_assert_uint_eq(PMEMBLK_MIN_BLK, bsize);
	const size_t nblock = pmemblk_nblock(p_);

	char buf[PMEMBLK_MIN_BLK];
	memset(buf, 0, sizeof(buf));

	/* negative block */
	errno = 0;
	pmemblk_read(p_, buf, -1LL);
	error(EINVAL);
	errno = 0;
	pmemblk_write(p_, buf, -1LL);
	error(EINVAL);
	errno = 0;
	pmemblk_set_zero(p_, -1LL);
	error(EINVAL);
	errno = 0;
	pmemblk_set_error(p_, -1LL);
	error(EINVAL);

	/* out-of-range block */
	errno = 0;
	pmemblk_read(p_, buf, nblock);
	error(EINVAL);
	errno = 0;
	pmemblk_write(p_, buf, nblock);
	error(EINVAL);
	errno = 0;
	pmemblk_set_zero(p_, nblock);
	error(EINVAL);
	errno = 0;
	pmemblk_set_error(p_, nblock);
	error(EINVAL);
}
END_TEST

START_TEST(open_OK)
{
	p_ = pmemblk_create_default(FILE_A);
	ck_assert_ptr_nonnull(p_);

	const size_t bsize = pmemblk_bsize(p_);
	ck_assert_uint_eq(PMEMBLK_MIN_BLK, bsize);
	const size_t nblock = pmemblk_nblock(p_);
	ck_assert_uint_lt(0, nblock);
	ck_assert_uint_gt(PMEMBLK_MIN_POOL, bsize * nblock);

	/* write ones to the block 0 */
	char obuf[PMEMBLK_MIN_BLK];
	memset(obuf, 0xFF, sizeof(obuf));
	success(pmemblk_write(p_, obuf, 0LL));

	/* set the error flag to the block 1 */
	success(pmemblk_set_error(p_, 1LL));

	/* re-open without block size verification */
	errno = 0;
	pmemblk_close(p_);
	error(0);
	p_ = pmemblk_open(FILE_A, 0); /* no verification */
	ck_assert_ptr_nonnull(p_);
	ck_assert_uint_eq(bsize, pmemblk_bsize(p_));
	ck_assert_uint_eq(nblock, pmemblk_nblock(p_));

	/* read ones from the block 0 */
	char buf[PMEMBLK_MIN_BLK];
	success(pmemblk_read(p_, buf, 0LL));
	ck_assert_mem_eq(obuf, buf, PMEMBLK_MIN_BLK);

	/* try to read from the block 1 but fail */
	errno = 0;
	failure(pmemblk_read(p_, buf, 1LL));
	error(EIO);

	/* re-open with block size verification */
	errno = 0;
	pmemblk_close(p_);
	error(0);
	p_ = pmemblk_open(FILE_A, bsize); /* verify block size */
	ck_assert_ptr_nonnull(p_);
}
END_TEST

START_TEST(open_EINVAL)
{
	p_ = pmemblk_create_default(FILE_A);
	ck_assert_ptr_nonnull(p_);

	const size_t bsize = pmemblk_bsize(p_);
	ck_assert_uint_eq(PMEMBLK_MIN_BLK, bsize);

	errno = 0;
	pmemblk_close(p_);
	error(0);
	p_ = NULL;

	/* block size verification failed */
	ck_assert_ptr_null(pmemblk_open(FILE_A, bsize * 2));
	error(EINVAL);
}
END_TEST

START_TEST(open_ENOENT)
{
	errno = 0;
	ck_assert_ptr_null(pmemblk_open(FILE_A, 0));
	error(ENOENT);
}
END_TEST

int main()
{
	TCase *const tcase_dax = tcase_create("DAX");
        tcase_add_unchecked_fixture(tcase_dax, setup_once_daxfs, NULL);
        tcase_add_checked_fixture(tcase_dax, setup, teardown);
        tcase_add_test(tcase_dax, create_OK);
        tcase_add_test(tcase_dax, create_OK_bsize_8KiB);
        tcase_add_test(tcase_dax, create_EINVAL_bsize);
        tcase_add_test(tcase_dax, create_EEXIST_pmemblk);
        tcase_add_test(tcase_dax, create_EEXIST_fd);
        tcase_add_test(tcase_dax, header_PMEMBLK);
        tcase_add_test(tcase_dax, open_OK);
        tcase_add_test(tcase_dax, open_EINVAL);
        tcase_add_test(tcase_dax, open_ENOENT);
        tcase_add_test(tcase_dax, readwrite_EINVAL);

	TCase *const tcase_nondax = tcase_create("non-DAX");
        tcase_add_unchecked_fixture(tcase_nondax, setup_once_nondaxfs, NULL);
        tcase_add_checked_fixture(tcase_nondax, setup, teardown);
        tcase_add_test(tcase_nondax, create_OK);
        tcase_add_test(tcase_nondax, create_OK_bsize_8KiB);
        tcase_add_test(tcase_nondax, create_EINVAL_bsize);
        tcase_add_test(tcase_nondax, create_EEXIST_pmemblk);
        tcase_add_test(tcase_nondax, create_EEXIST_fd);
        tcase_add_test(tcase_nondax, header_PMEMBLK);
        tcase_add_test(tcase_nondax, open_OK);
        tcase_add_test(tcase_nondax, open_EINVAL);
        tcase_add_test(tcase_nondax, open_ENOENT);
        tcase_add_test(tcase_dax, readwrite_EINVAL);

	Suite *const suite = suite_create("libpmemblk");

	char *const env_force = getenv("PMEM_IS_PMEM_FORCE");
	const int force = env_force ? atoi(env_force) : -1;
	if (force != 0)
		suite_add_tcase(suite, tcase_dax);
#if 0
	/* FIXME This cannot pass... */
	if (force != 1)
		suite_add_tcase(suite, tcase_nondax);
#endif

	SRunner *const srunner = srunner_create(suite);
	srunner_run_all(srunner, CK_NORMAL);
	const int failed = srunner_ntests_failed(srunner);
	srunner_free(srunner);

	return !!failed;
}
