#include <check.h>
#include <errno.h>
#include <libpmemlog.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define zero(actual) ck_assert_int_eq(0, (actual))
#define cmpeq(actual) ck_assert_int_eq(0, (actual))
#define success(actual) ck_assert_int_eq(0, (actual))
#define failure(actual) ck_assert_int_eq(-1, (actual))
#define error(expected) ck_assert_int_eq((expected), errno)

static PMEMlogpool *p_, *q_;

void setup_once_dax(void)
{
	p_ = NULL;
	q_ = NULL;
	success(chdir("/mnt/pmem0/tmp"));
}

void setup_once_nondax(void)
{
	p_ = NULL;
	q_ = NULL;
	success(chdir("/tmp"));
}

void setup(void)
{
	unlink("hoge"); // DO NOT assert
	unlink("fuga"); // DO NOT assert

	failure(access("hoge", F_OK));
	error(ENOENT);
	failure(access("fuga", F_OK));
	error(ENOENT);
}

void teardown(void)
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

START_TEST(test_create)
{
	p_ = pmemlog_create("hoge", PMEMLOG_MIN_POOL, 0644);
	ck_assert(p_);
	zero(pmemlog_tell(p_));

	const size_t size = pmemlog_nbyte(p_);
	ck_assert_uint_lt(0, size);

	void *const buf = calloc(size, 1);
	ck_assert(buf);

	success(pmemlog_append(p_, buf, size));
	ck_assert_int_eq((long long)size, pmemlog_tell(p_));
	ck_assert_uint_eq(size, pmemlog_nbyte(p_));

	failure(pmemlog_append(p_, buf, 1));
	error(ENOSPC);
	ck_assert_int_eq((long long)size, pmemlog_tell(p_));
	ck_assert_uint_eq(size, pmemlog_nbyte(p_));

	pmemlog_rewind(p_);
	zero(pmemlog_tell(p_));
	ck_assert_uint_eq(size, pmemlog_nbyte(p_));

	free(buf);
}
END_TEST

START_TEST(test_header)
{
	p_ = pmemlog_create("hoge", PMEMLOG_MIN_POOL, 0644);
	ck_assert(p_);
	pmemlog_close(p_);
	p_ = NULL;

	FILE *const fp = fopen("hoge", "r+b");
	ck_assert(fp);

	char header[8]; // including NUL
	ck_assert_uint_eq(8, fread(header, sizeof(char), 8, fp));
	cmpeq(memcmp("PMEMLOG", header, 8));

	success(fclose(fp));
}
END_TEST

START_TEST(test_open)
{
	p_ = pmemlog_create("hoge", PMEMLOG_MIN_POOL, 0644);
	ck_assert(p_);
	zero(pmemlog_tell(p_));

	const size_t size = pmemlog_nbyte(p_);
	ck_assert_uint_lt(0, size);

	void *const buf = calloc(size/2, 1);
	ck_assert(buf);

	success(pmemlog_append(p_, buf, size/2));
	ck_assert_int_eq((long long)(size/2), pmemlog_tell(p_));

	pmemlog_close(p_);

	p_ = pmemlog_open("hoge");
	ck_assert(p_);
	ck_assert_int_eq((long long)(size/2), pmemlog_tell(p_));

	failure(pmemlog_append(p_, buf, size));
	error(ENOSPC);
	ck_assert_int_eq((long long)(size/2), pmemlog_tell(p_));

	pmemlog_rewind(p_);
	zero(pmemlog_tell(p_));

	pmemlog_close(p_);
	p_ = pmemlog_open("hoge");
	ck_assert(p_);
	zero(pmemlog_tell(p_));
}
END_TEST

START_TEST(test_unlink)
{
	p_ = pmemlog_create("hoge", PMEMLOG_MIN_POOL, 0644);
	ck_assert(p_);
	zero(pmemlog_tell(p_));

	success(access("hoge", F_OK));
	success(unlink("hoge"));
	failure(access("hoge", F_OK));
	error(ENOENT);
}
END_TEST

int assert_walk_test_rename(const void *, size_t, void *);
START_TEST(test_rename)
{
	p_ = pmemlog_create("hoge", PMEMLOG_MIN_POOL, 0644);
	ck_assert(p_);

	q_ = pmemlog_create("fuga", PMEMLOG_MIN_POOL, 0644);
	ck_assert(q_);

	void *const old_data = malloc(8192);
	ck_assert(old_data);
	memset(old_data, 0x00, 8192);

	void *const new_data = malloc(4096);
	ck_assert(new_data);
	memset(new_data, 0xFF, 4096);

	success(pmemlog_append(p_, old_data, 8192));
	success(pmemlog_append(q_, new_data, 4096));
	free(old_data);
	free(new_data);

	pmemlog_close(q_);
	q_ = pmemlog_open("fuga");
	ck_assert(q_);
	ck_assert_int_eq(4096LL, pmemlog_tell(q_));

	success(rename("fuga", "hoge")); // mv fuga hoge
	success(access("hoge", F_OK));
	failure(access("fuga", F_OK));
	error(ENOENT);

	PMEMlogpool *const t = p_;
	p_ = q_;
	q_ = NULL;
	pmemlog_close(t);

	ck_assert_int_eq(4096LL, pmemlog_tell(p_));
	pmemlog_walk(p_, 0, assert_walk_test_rename, NULL);
}
END_TEST

int assert_walk_test_rename(const void *buf, size_t len, void *arg)
{
	(void)arg;

	void *const expected = malloc(4096);
	ck_assert(expected);
	memset(expected, 0xFF, 4096);

	ck_assert_uint_eq(len, 4096);
	cmpeq(memcmp(expected, buf, 4096));

	free(expected);
	return 0; // terminate the walk
}

int main()
{
	TCase *const tcase1 = tcase_create("DAX");
	tcase_add_unchecked_fixture(tcase1, setup_once_dax, NULL);
	tcase_add_checked_fixture(tcase1, setup, teardown);
	tcase_add_test(tcase1, test_create);
	tcase_add_test(tcase1, test_header);
	tcase_add_test(tcase1, test_open);
	tcase_add_test(tcase1, test_unlink);
	tcase_add_test(tcase1, test_rename);

	TCase *const tcase2 = tcase_create("non-DAX");
	tcase_add_unchecked_fixture(tcase2, setup_once_nondax, NULL);
	tcase_add_checked_fixture(tcase2, setup, teardown);
	tcase_add_test(tcase2, test_create);
	tcase_add_test(tcase2, test_header);
	tcase_add_test(tcase2, test_open);
	tcase_add_test(tcase2, test_unlink);
	tcase_add_test(tcase2, test_rename);

	Suite *const suite = suite_create("libpmemlog");
	suite_add_tcase(suite, tcase1);
	suite_add_tcase(suite, tcase2);

	SRunner *const srunner = srunner_create(suite);
	srunner_run_all(srunner, CK_NORMAL);
	const int failed = srunner_ntests_failed(srunner);
	srunner_free(srunner);

	return !!failed;
}
