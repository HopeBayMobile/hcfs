/**************************************************************************
 *
 * Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
 *
 * File Name: monitor.c
 * Abstract: The c source code file for monitor backend thread and
 *           sync control (upload/download/delete).
 *
 * Revision History
 * 2015/10/30 Jethro
 *
 *************************************************************************/

#include "monitor.h"

#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/prctl.h>

#include "fuseop.h"
#include "global.h"
#include "logger.h"
#include "hcfscurl.h"
#include "macro.h"

CURL_HANDLE monitor_curl_handle;

/**************************************************************************
 *
 * Function name: monitor_loop
 *        Inputs: void *arg
 *       Summary: Main function for checking cloud connection status.
 *                Upon successful return, this function return 1. which
 *                means cloud storage is reachable and user auth to
 *                storage is valid.
 *                If a connection error is encountered, a zero value is
 *                returned.
 *  Return value: None
 *
 *************************************************************************/
#ifdef _ANDROID_ENV_
void *monitor_loop(void *ptr)
#else
void monitor_loop(void)
#endif
{
	const struct timespec sleep_time = {1, 0}; /* sleep 1 second */
	struct timespec *last_time = &hcfs_system->backend_status_last_time;
	struct timespec test_start;
	struct timespec test_stop;
	struct timespec backend_idle_time;
	struct timespec test_duration;
	int32_t ret_val;
#ifdef _ANDROID_ENV_
	UNUSED(ptr);
	prctl(PR_SET_NAME, "monitor_loop");
#endif /* _ANDROID_ENV_ */

	monitor_curl_handle.curl_backend = NONE;
	monitor_curl_handle.curl = NULL;

	write_log(2, "[Backend Monitor] Start monitor loop\n");

	while (hcfs_system->system_going_down == FALSE) {
		clock_gettime(CLOCK_REALTIME, &test_start);
		backend_idle_time = diff_time(*last_time, test_start);
		if (backend_idle_time.tv_sec >= MONITOR_INTERVAL) {

			ret_val = hcfs_test_backend(&monitor_curl_handle);

			if ((ret_val >= 200) && (ret_val <= 299))
				hcfs_system->backend_status_is_online = TRUE;
			else
				hcfs_system->backend_status_is_online = FALSE;

			clock_gettime(CLOCK_REALTIME, &test_stop);
			test_duration = diff_time(test_start, test_stop);
			write_log(
			    10,
			    "[Backend Monitor] backend is %s, test time %f\n",
			    hcfs_system->backend_status_is_online ? "online"
								  : "offline",
			    test_duration.tv_sec +
				test_start.tv_nsec / 1000000000.0);
			last_time->tv_sec = test_start.tv_sec;
			last_time->tv_nsec = test_start.tv_nsec;
		}
		/* wait 0.1 second */
		ret_val = nanosleep(&sleep_time, NULL);
		if (ret_val == -1 && errno == EINTR) {
			write_log(2, "[Backend Monitor] interrupted\n");
			break;
		}
	}
#ifdef _ANDROID_ENV_
	return NULL;
#else
	return;
#endif
}

/**************************************************************************
 *
 * Function name: diff_time
 *        Inputs: timespec start
 *                timespec end
 *       Summary: Calculate time duration between [start] and [end]
 *  Return value: Type timespec, duration between [start] and [end]
 *
 *************************************************************************/
struct timespec diff_time(struct timespec start, struct timespec end)
{
	struct timespec temp;

	if ((end.tv_nsec - start.tv_nsec) < 0) {
		temp.tv_sec = end.tv_sec - start.tv_sec - 1;
		temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec - start.tv_sec;
		temp.tv_nsec = end.tv_nsec - start.tv_nsec;
	}
	return temp;
}
