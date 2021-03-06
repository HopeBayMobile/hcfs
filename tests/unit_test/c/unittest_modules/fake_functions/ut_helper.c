/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
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
	} while (0)
	FAKE_FUNC_LIST
#undef X
}

int32_t write_log_hide;

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
	write_log_hide = 11;
}

/*
 * Implementation of fake functions
 */
#undef write_log
char log_data[LOG_RECORD_SIZE][1024];
int32_t write_log(int32_t level, const char *format, ...)
{
	va_list alist;
	size_t len;
	int32_t log_idx;
	char output[4096];

	if (level >= write_log_hide)
		return 0;
	write_log_call_count++;
	va_start(alist, format);
	vsprintf(output, format, alist);
	va_end(alist);
	len = strlen(output);
	if (output[len - 1] != '\n') {
		output[len] = '\n';
		output[len + 1] = 0;
	}
	printf("%s", output);

	/* Cache and check logs. */
	va_start(alist, format);
	log_idx = write_log_call_count % LOG_RECORD_SIZE;
	vsprintf(log_data[log_idx], format, alist);
	va_end(alist);
	return 0;
}
