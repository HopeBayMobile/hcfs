/*************************************************************************
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
**************************************************************************/
#ifndef TESTS_UNIT_TEST_C_UNITTEST_MODULES_FUSEOP_UNITTESTS_UT_HELPER_H_
#define TESTS_UNIT_TEST_C_UNITTEST_MODULES_FUSEOP_UNITTESTS_UT_HELPER_H_

#include <semaphore.h>
#include <inttypes.h>

void reset_ut_helper(void);


typedef int(sem_wait_f)(sem_t *);
typedef int(sem_post_f)(sem_t *);
typedef int32_t(write_log_f)(int32_t, const char *, ...);
extern char log_data[5][1024];
extern int32_t write_log_hide;
#define PD(x)                                                                  \
	if (write_log_hide > 10)                                               \
	printf(#x " %d\n", x)

#define write_log(...) write_log_ptr(__VA_ARGS__)
#define sem_wait(x) sem_wait_ptr(x)
#define sem_post(x) sem_post_ptr(x)

#define FKAE_FUNC_LIST                                                         \
	X(write_log);                                                          \
	X(sem_wait);                                                           \
	X(sem_post);

#define DECLARE_FAKE_SET(func)                                                 \
	extern func##_f *func##_ptr;                                           \
	extern uint32_t func##_error_on;                                       \
	extern uint32_t func##_call_count;                                     \
	func##_f func##_cnt;
#define X(func) DECLARE_FAKE_SET(func)
FKAE_FUNC_LIST
#undef X



#endif /* TESTS_UNIT_TEST_C_UNITTEST_MODULES_FUSEOP_UNITTESTS_UT_HELPER_H_ */
