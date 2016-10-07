/*************************************************************************
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
**************************************************************************/
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fuse/fuse_lowlevel.h>
#include <stdarg.h>
#include <stdio.h>

#include "ut_helper.h"

/*
 * Template for fake functions
 */
#define X(func)                                                                \
	typeof(func) *func##_real;                                             \
	uint32_t func##_error_on;                                              \
	uint32_t func##_call_count;                                            \
	int32_t func##_errno
FAKE_FUNC_LIST
#undef X

/* This will always run before main() */
void __attribute__((constructor)) Init(void)
{
	/* Call back to origin function until they are faked */
#define X(func)                                                                \
	do {                                                                   \
		func##_real = (typeof(func) *)dlsym(RTLD_NEXT, #func);         \
		if (!func##_real && strcmp(#func, "write_log") != 0)           \
			func##_real = func;                                    \
		printf(#func " %p\n", func##_real);                            \
	} while (0)
	FAKE_FUNC_LIST
#undef X
}

void reset_ut_helper(void)
{
#define X(func)                                                                \
	do {                                                                   \
		func##_call_count = 0;                                         \
		func##_error_on = -1;                                          \
		func##_errno = 0;                                              \
	} while (0)
	FAKE_FUNC_LIST
#undef X
	sem_init_errno = EINVAL;
	strndup_errno = ENOMEM;
	malloc_errno = ENOMEM;
}

/*
 * Implementation of fake functions
 */

char log_data[LOG_RECORD_SIZE][1024];
int32_t write_log_hide = 11;
int32_t write_log_wrap(int32_t level, const char *format, ...)
{
	va_list alist;

	if (level >= write_log_hide)
		return 0;
	write_log_call_count++;
	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);

	va_start(alist, format);
	vsprintf(log_data[write_log_call_count % LOG_RECORD_SIZE], format,
		 alist);
	va_end(alist);
	return 0;
}
