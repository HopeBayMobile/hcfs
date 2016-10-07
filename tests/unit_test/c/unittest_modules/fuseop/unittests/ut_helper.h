/*************************************************************************
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
**************************************************************************/
#ifndef TESTS_UNIT_TEST_C_UNITTEST_MODULES_FUSEOP_UNITTESTS_UT_HELPER_H_
#define TESTS_UNIT_TEST_C_UNITTEST_MODULES_FUSEOP_UNITTESTS_UT_HELPER_H_

#include <fuse/fuse_lowlevel.h>
#include <inttypes.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>

void reset_ut_helper(void);

/*
 * Overwrite write_log
 */
#if !defined(LOG_RECORD_SIZE)
#define LOG_RECORD_SIZE 10
#endif
extern char log_data[LOG_RECORD_SIZE][1024];
extern int32_t write_log_hide;
int32_t write_log(int32_t, const char *, ...);
typeof(write_log) write_log_wrap;
#define PD(x)                                                                  \
	do {                                                                   \
		if (write_log_hide > 10)                                       \
			printf(#x " %d\n", x);                                 \
	} while (0)

/*
 * List of All faked functions
 */
#define FAKE_FUNC_LIST                                                         \
	X(write_log);                                                          \
	X(sem_init);                                                           \
	X(sem_wait);                                                           \
	X(sem_post);                                                           \
	X(pthread_create);                                                     \
	X(strndup);                                                            \
	X(fuse_lowlevel_notify_delete);                                        \
	X(malloc);                                                             \
	X(realloc);

/*
 * Fake library functions
 * #define FUNC_NAME(...) _ut_wrap(FUNC_NAME, Return_On_Error, __VA_ARGS__)
 */
#define write_log(...) write_log_wrap(__VA_ARGS__)

#define sem_init(...) _ut_wrap(sem_init, -1, __VA_ARGS__)
#define sem_wait(...) _ut_wrap(sem_wait, -1, __VA_ARGS__)
#define sem_post(...) _ut_wrap(sem_post, -1, __VA_ARGS__)
#define pthread_create(...) _ut_wrap(pthread_create, EAGAIN, __VA_ARGS__)
#define strndup(...) _ut_wrap(strndup, NULL, __VA_ARGS__)
#define fuse_lowlevel_notify_delete(...)                                       \
	_ut_wrap(fuse_lowlevel_notify_delete, -ENOENT, __VA_ARGS__)
#define malloc(...) _ut_wrap(malloc, NULL, __VA_ARGS__)
#define realloc(...) _ut_wrap(realloc, NULL, __VA_ARGS__)

/*
 * Declare assets of fake functions
 */
#define X(func)                                                                \
	extern typeof(func) *func##_real;                                      \
	extern uint32_t func##_error_on;                                       \
	extern uint32_t func##_call_count;                                     \
	extern int32_t func##_errno;
FAKE_FUNC_LIST
X(write_log);
#undef X

#define _ut_wrap(func, error_ret, ...)                                         \
	({                                                                     \
		func##_call_count++;                                           \
		PD(func##_call_count);                                         \
		if (func##_call_count == func##_error_on)                      \
			errno = func##_errno;                                  \
		(func##_call_count != func##_error_on)                         \
		    ? func##_real(__VA_ARGS__)                                 \
		    : error_ret;                                               \
	})


#endif /* TESTS_UNIT_TEST_C_UNITTEST_MODULES_FUSEOP_UNITTESTS_UT_HELPER_H_ */
