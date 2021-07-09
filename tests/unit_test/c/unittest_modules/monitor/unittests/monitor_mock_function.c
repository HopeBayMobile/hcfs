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

int32_t update_use_minimal_apk(void)
{
	return 0;
}
