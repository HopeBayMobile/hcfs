/* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved. */
#include "logger.h"
#include "hcfscurl.h"
#include "time.h"

#define UNUSED(x) ((void)x)

int write_log(int level, char *format, ...)
{
	UNUSED(level);
	UNUSED(format);
	return 0;
}

int hcfs_test_backend_register = 401;
int hcfs_test_backend(CURL_HANDLE *curl_handle)
{
	UNUSED(curl_handle);
	return hcfs_test_backend_register;
}
