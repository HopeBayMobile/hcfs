/**************************************************************************
 *
 * Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
 *
 * File Name: monitor.c
 * Abstract: The c source code file for monitor backend connection thread and
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

CURL_HANDLE monitor_curl_handle;

/**************************************************************************
 *
 * Function name: monitor_loop
 *        Inputs: void *arg
 *       Summary: Main function for checking whether there is a need to
 *                delete objects from backend.
 *  Return value: None
 *
 *************************************************************************/
void monitor_loop()
{
	struct timespec timenow;
	struct timespec idle_time;
	struct timespec _100_millisecond = {0, 100 * 1000000};
	int ret;
	monitor_curl_handle.curl_backend = NONE;
	monitor_curl_handle.curl = NULL;

	write_log(2, "Start monitor loop\n");

#ifdef _ANDROID_ENV_
	prctl(PR_SET_NAME, "monitor_loop");
#endif /* _ANDROID_ENV_ */

	while (hcfs_system->system_going_down == FALSE) {
		clock_gettime(CLOCK_REALTIME, &timenow);
		idle_time = diff_time(hcfs_system->access_time, timenow);
		if (idle_time.tv_sec >= MONITOR_INTERVAL) {
			/* TODO: check backend */

		}
		/* wait 0.1 second */
		ret = nanosleep(&_100_millisecond, NULL);
		if (ret == -1 && errno == EINTR) {
			write_log(2, "monitor_loop is interrupted by signal\n");
			break;
		}
	}
	return;
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
