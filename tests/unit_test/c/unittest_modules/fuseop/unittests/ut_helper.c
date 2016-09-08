/*************************************************************************
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
**************************************************************************/
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include "ut_helper.h"

/*
 * Template for fake functions
 */
#define DEFINE_FAKE_SET(func)                                                  \
	func##_f(*func##_ptr);                                                 \
	func##_f(*func##_real);                                                \
	uint32_t func##_error_on = -1;                                         \
	uint32_t func##_call_count

#define X(func) DEFINE_FAKE_SET(func)
FKAE_FUNC_LIST
#undef X

/* This will always run before main() */
void __attribute__((constructor)) Init(void)
{
	/* Call back to origin function until they are faked */
#define X(func) func##_real = func##_ptr = (func##_f *)dlsym(RTLD_NEXT, #func)
	FKAE_FUNC_LIST
#undef X
}

void reset_ut_helper(void)
{
#define X(func)                                                                \
	func##_call_count = 0;                                                 \
	func##_error_on = -1;                                                  \
	func##_ptr = func##_cnt
	FKAE_FUNC_LIST
#undef X
}

/*
 * Implementation of fake functions
 */

int32_t sem_wait_cnt(sem_t *sem)
{
	sem_wait_call_count++;
	PD(sem_wait_call_count);
	if (sem_wait_call_count != sem_wait_error_on)
		return sem_wait_real(sem);
	else
		return -1;
}

int32_t sem_post_cnt(sem_t *sem)
{
	sem_post_call_count++;
	PD(sem_post_call_count);
	if (sem_post_call_count != sem_post_error_on)
		return sem_post_real(sem);
	else
		return -1;
}

char log_data[5][1024];
int32_t write_log_hide = 11;
int32_t write_log_cnt(int32_t level, const char *format, ...)
{
	va_list alist;

	if (level >= write_log_hide)
		return 0;
	write_log_call_count++;
	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);

	va_start(alist, format);
	vsprintf(log_data[write_log_call_count % 5], format, alist);
	va_end(alist);
	return 0;
}

int pthread_create_cnt(pthread_t *thread,
		       const pthread_attr_t *attr,
		       void *(*start_routine)(void *),
		       void *arg)
{
	pthread_create_call_count++;
	PD(pthread_create_call_count);
	if (pthread_create_call_count != pthread_create_error_on) {
		return pthread_create_real(thread, attr, start_routine, arg);
	} else {
		return EAGAIN;
	}

	return 0;
}

char *strndup_cnt(const char *s, size_t n)
{
	strndup_call_count++;
	PD(strndup_call_count);
	if (strndup_call_count != strndup_error_on) {
		return strndup_real(s, n);
	} else {
		errno = ENOMEM;
		return NULL;
	}
}

