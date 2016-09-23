/*************************************************************************
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
**************************************************************************/
#ifndef TESTS_UNIT_TEST_C_UNITTEST_MODULES_FUSEOP_UNITTESTS_UT_HELPER_H_
#define TESTS_UNIT_TEST_C_UNITTEST_MODULES_FUSEOP_UNITTESTS_UT_HELPER_H_

#include <semaphore.h>
#include <inttypes.h>

void reset_ut_helper(void);

/* 
 * Type define of fake function 
 */
typedef int(sem_wait_f)(sem_t *);
#define sem_wait(...) sem_wait_ptr(__VA_ARGS__)

typedef int(sem_post_f)(sem_t *);
#define sem_post(...) sem_post_ptr(__VA_ARGS__)

typedef int32_t(write_log_f)(int32_t, const char *, ...);
#define write_log(...) write_log_ptr(__VA_ARGS__)
extern char log_data[5][1024];
extern int32_t write_log_hide;

typedef int (pthread_create_f)(pthread_t *thread,
		   const pthread_attr_t *attr,
		   void *(*start_routine)(void *),
		   void *arg);
#define pthread_create(...) pthread_create_ptr(__VA_ARGS__)

typedef char *(strndup_f)(const char *s, size_t n);
#define strndup(...) strndup_ptr(__VA_ARGS__)

#define FKAE_FUNC_LIST                                                         \
	X(sem_wait);                                                           \
	X(sem_post);                                                           \
	X(write_log);                                                          \
	X(pthread_create);                                                     \
	X(strndup);

#define DECLARE_FAKE_SET(func)                                                 \
	extern func##_f *func##_ptr;                                           \
	extern uint32_t func##_error_on;                                       \
	extern uint32_t func##_call_count;                                     \
	func##_f func##_cnt;
#define X(func) DECLARE_FAKE_SET(func)
FKAE_FUNC_LIST
#undef X

#define PD(x)                                                                  \
	if (write_log_hide > 10)                                               \
	printf(#x " %d\n", x)


#endif /* TESTS_UNIT_TEST_C_UNITTEST_MODULES_FUSEOP_UNITTESTS_UT_HELPER_H_ */
