/* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved. */

#include "mock_params.h"
#include "logger.h"
#include "hcfscurl.h"
#include "time.h"

#define UNUSED(x) ((void)x)

int32_t write_log(int32_t level, const char *format, ...)
{
	va_list alist;

	UNUSED(level);
	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

int32_t hcfs_test_backend_register = 401;
int32_t hcfs_test_backend_sleep_nsec = 0;
int32_t hcfs_test_backend(CURL_HANDLE *curl_handle)
{
	struct timespec larger_than_interval;

	UNUSED(curl_handle);
	larger_than_interval.tv_sec = 0;
	larger_than_interval.tv_nsec = hcfs_test_backend_sleep_nsec;
	nanosleep(&larger_than_interval, NULL);
	return hcfs_test_backend_register;
}

void notify_sleep_on_cache(int32_t cache_replace_status)
{
	mock_status = cache_replace_status;
}

void wake_sb_rebuilder(void)
{
	return;
}

