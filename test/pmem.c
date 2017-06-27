#include <check.h>
#include <errno.h>
#include <libpmem.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define success(actual) ck_assert_int_eq(0, (actual))
#define error(expected) ck_assert_int_eq((expected), errno)
#define cmpeq(actual) ck_assert_int_eq(0, (actual))

#ifndef DIR_DAX
#define DIR_DAX "/mnt/pmem0/tmp"
#endif
#ifndef DIR_NONDAX
#define DIR_NONDAX "/tmp"
#endif
#ifndef FILE_TMP
#define FILE_TMP "foobar"
#endif
#ifndef PATH_DEVICE_DAX
#define PATH_DEVICE_DAX "/dev/dax1.0"
#endif

#undef  PATH_DAX_FILE
#define PATH_DAX_FILE (DIR_DAX "/" FILE_TMP)

#undef  PATH_NONDAX_FILE
#define PATH_NONDAX_FILE (DIR_NONDAX "/" FILE_TMP)

static int force_ = -1;

static void setup_once_dax_fs(void)
{
	/* Checks whether tmp on DAX FS is accessible or not. */
	success(access(DIR_DAX, R_OK|W_OK|X_OK));
}

static void setup_dax_fs(void)
{
	unlink(PATH_DAX_FILE); /* DO NOT assert */
}

static void setup_once_nondax_fs(void)
{
	/* Checks whether tmp on non-DAX FS is accessible or not. */
	success(access(DIR_NONDAX, R_OK|W_OK|X_OK));
}

static void setup_nondax_fs(void)
{
	unlink(PATH_NONDAX_FILE); /* DO NOT assert */
}

static void setup_once_device_dax(void)
{
	/* Checks whether PATH_DEVICE_DAX is accessible or not. */
	success(access(PATH_DEVICE_DAX, R_OK|W_OK));

	/* Checks whether PATH_DEVICE_DAX is actually Device DAX or not. */
	struct stat st = {0};
	success(stat(PATH_DEVICE_DAX, &st));
	ck_assert(S_ISCHR(st.st_mode));

	const int maj = (int)major(st.st_rdev), min = (int)minor(st.st_rdev);
	char spath[PATH_MAX], npath[PATH_MAX];
	snprintf(spath, PATH_MAX, "/sys/dev/char/%d:%d/subsystem", maj, min);
	const char *const rpath = realpath(spath, npath);
	cmpeq(strcmp("/sys/class/dax", rpath));
}

START_TEST(is_pmem_dax_fs)
{
	static const size_t len = 2*1024*1024;
	static const int flags = PMEM_FILE_CREATE|PMEM_FILE_EXCL;

	size_t mapped_len = 0;
	int is_pmem = 0;
	void *const addr = pmem_map_file(
		PATH_DAX_FILE, len, flags, 0600, &mapped_len, &is_pmem);

	ck_assert(!!addr);
	ck_assert_uint_eq(len, mapped_len);
	/*
	 * PMEM_IS_PMEM_FORCE=1 implies is_pmem is true.
	 * Note that is_pmem becomes *false* even in a case of DAX FS.
	 */
	ck_assert(!(force_ == 1) || is_pmem);

	success(pmem_unmap(addr, mapped_len));
}
END_TEST

START_TEST(is_pmem_nondax_fs)
{
	static const size_t len = 2*1024*1024;
	static const int flags = PMEM_FILE_CREATE|PMEM_FILE_EXCL;

	size_t mapped_len = 0;
	int is_pmem = 0;
	void *const addr = pmem_map_file(
		PATH_NONDAX_FILE, len, flags, 0600, &mapped_len, &is_pmem);

	ck_assert(!!addr);
	ck_assert_uint_eq(len, mapped_len);
	/*
	 * PMEM_IS_PMEM_FORCE=1 implies is_pmem is true even if non-DAX FS.
	 * Note that is_pmem becomes false in a case of non-DAX FS.
	 */
	ck_assert(!(force_ == 1) || is_pmem);

	success(pmem_unmap(addr, mapped_len));
}
END_TEST

START_TEST(is_pmem_device_dax)
{
	size_t mapped_len = 0;
	int is_pmem = 0;
	void *const addr = pmem_map_file(
		PATH_DEVICE_DAX, 0, 0, 0600, &mapped_len, &is_pmem);

	ck_assert(!!addr);
	ck_assert_uint_lt(0, mapped_len);
	/* DO NOT care PMEM_IS_PMEM_FORCE. */
	ck_assert(is_pmem);

	success(pmem_unmap(addr, mapped_len));
}
END_TEST

START_TEST(pmem_map_file_len_EINVAL_device_dax)
{
	/*
	 * Non-zero "len" causes EINVAL
	 * if it does not equal to actual size of Device DAX.
	 */
	static const size_t len = 2*1024*1024;

	ck_assert(!pmem_map_file(PATH_DEVICE_DAX, len, 0, 0600, NULL, NULL));
	error(EINVAL);
}
END_TEST

START_TEST(pmem_map_file_flags_EINVAL_device_dax)
{
	/*
	 * "flags" containing PMEM_FILE_EXCL causes EINVAL.
	 */
	static const int flags = PMEM_FILE_CREATE|PMEM_FILE_EXCL;

	ck_assert(!pmem_map_file(PATH_DEVICE_DAX, 0, flags, 0600, NULL, NULL));
	error(EINVAL);
}
END_TEST

int main()
{
	/* Checks environment variable(s). */
	const char *const p = getenv("PMEM_IS_PMEM_FORCE");
	if (p && (strcmp(p, "0") == 0 || strcmp(p, "1") == 0))
		force_ = atoi(p);

	TCase *const tcase1 = tcase_create("dax_fs");
	tcase_add_unchecked_fixture(tcase1, setup_once_dax_fs, NULL);
	tcase_add_checked_fixture(tcase1, setup_dax_fs, NULL);
	tcase_add_test(tcase1, is_pmem_dax_fs);

	TCase *const tcase2 = tcase_create("nondax_fs");
	tcase_add_unchecked_fixture(tcase2, setup_once_nondax_fs, NULL);
	tcase_add_checked_fixture(tcase2, setup_nondax_fs, NULL);
	tcase_add_test(tcase2, is_pmem_nondax_fs);

	TCase *const tcase3 = tcase_create("device_dax");
	tcase_add_unchecked_fixture(tcase3, setup_once_device_dax, NULL);
	tcase_add_test(tcase3, is_pmem_device_dax);
	tcase_add_test(tcase3, pmem_map_file_len_EINVAL_device_dax);
	tcase_add_test(tcase3, pmem_map_file_flags_EINVAL_device_dax);

	Suite *const suite = suite_create("libpmem");
	suite_add_tcase(suite, tcase1);
	suite_add_tcase(suite, tcase2);
	suite_add_tcase(suite, tcase3);

	SRunner *const srunner = srunner_create(suite);
	srunner_run_all(srunner, CK_NORMAL);
	const int failed = srunner_ntests_failed(srunner);
	srunner_free(srunner);

	return !!failed;
}
