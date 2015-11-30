#include "logger.h"
#include "hcfscurl.h"
#include "time.h"

int write_log(int level, char *format, ...)
{
	return 0;
}

int hcfs_test_backend(CURL_HANDLE *curl_handle)
{
	return 0;
}

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
	return 0;
}
