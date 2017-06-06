#include <check.h>
#include <libpmem.h>
#include <stdlib.h>

#define success(actual) ck_assert_int_eq(0, (actual))

#define OVERWRITE_YES 1

START_TEST(test_pmem_is_pmem_never)
{
	success(setenv("PMEM_IS_PMEM_FORCE", "0", OVERWRITE_YES));
	ck_assert(!pmem_is_pmem(NULL, 0));
}
END_TEST

START_TEST(test_pmem_is_pmem_always)
{
	success(setenv("PMEM_IS_PMEM_FORCE", "1", OVERWRITE_YES));
	ck_assert(pmem_is_pmem(NULL, 0));
}
END_TEST

int main()
{
	TCase *const tcase = tcase_create("all");
	tcase_add_test(tcase, test_pmem_is_pmem_never);
	tcase_add_test(tcase, test_pmem_is_pmem_always);

	Suite *const suite = suite_create("libpmem");
	suite_add_tcase(suite, tcase);

	SRunner *const srunner = srunner_create(suite);
	srunner_run_all(srunner, CK_NORMAL);
	const int failed = srunner_ntests_failed(srunner);
	srunner_free(srunner);

	return !!failed;
}
