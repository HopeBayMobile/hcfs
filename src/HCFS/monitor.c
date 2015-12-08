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
#include "params.h"

CURL_HANDLE monitor_curl_handle;
/* let it can be changed during unittest*/
int monitoring_interval = MONITORING_INTERVAL;

static inline void _write_monitor_loop_status_log(float duration)
{
	static BOOL init = TRUE;
	static BOOL backend_is_online;
	static BOOL sync_manual_switch;
	static BOOL sync_paused;

	if (init) {
		backend_is_online = !hcfs_system->backend_is_online;
		sync_manual_switch = !hcfs_system->sync_manual_switch;
		sync_paused = !hcfs_system->sync_paused;
		init = FALSE;
	}
	/* log about sleep & resume */
	if (backend_is_online != hcfs_system->backend_is_online ||
	    sync_manual_switch != hcfs_system->sync_manual_switch ||
	    sync_paused != hcfs_system->sync_paused) {
		backend_is_online = hcfs_system->backend_is_online;
		sync_manual_switch = hcfs_system->sync_manual_switch;
		sync_paused = hcfs_system->sync_paused;
		if (duration)
			write_log(10, "%s backend [%s] (time %.3f)\n",
				  "Debug: [Monitor]",
				  backend_is_online ? "online" : "offline",
				  duration);
		else
			write_log(10, "Debug: [Monitor] backend [%s]\n",
				  backend_is_online ? "online" : "offline");
		write_log(10, "Debug: [Monitor] sync switch [%s]\n",
			  sync_manual_switch ? "on" : "off");
		write_log(10, "Debug: [Monitor] hcfs sync state [%s]\n",
			  sync_paused ? "paused" : "syncing");
	}
}
/**************************************************************************
 *
 * Function name: monitor_loop
 *        Inputs: void *arg
 *       Summary: Main function for checking backend connection status.
 *                Upon successful return, this function return 1. which
 *                means backend storage is reachable and user auth to
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
	struct timespec loop_start_time;
	struct timespec test_stop;
	float test_duration;
	float idle_time;
	int32_t ret_val;
#ifdef _ANDROID_ENV_
	UNUSED(ptr);
	prctl(PR_SET_NAME, "monitor_loop");
#endif /* _ANDROID_ENV_ */

	monitor_curl_handle.curl_backend = NONE;
	monitor_curl_handle.curl = NULL;

	write_log(2, "[Monitor] Start monitor loop\n");

	while (hcfs_system->system_going_down == FALSE) {
		clock_gettime(CLOCK_REALTIME, &loop_start_time);
		idle_time = diff_time(*last_time, loop_start_time);

		test_duration = 0;
		if (idle_time >= monitoring_interval) {
			ret_val = hcfs_test_backend(&monitor_curl_handle);
			if ((ret_val >= 200) && (ret_val <= 299))
				update_backend_status(TRUE, &loop_start_time);
			else
				update_backend_status(FALSE, &loop_start_time);

			clock_gettime(CLOCK_REALTIME, &test_stop);
			test_duration = diff_time(loop_start_time, test_stop);
		}
		_write_monitor_loop_status_log(test_duration);
		/* wait 1 second */
		nanosleep(&sleep_time, NULL);
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
 *  Return value: float, duration between [start] and [end]
 *
 *************************************************************************/
inline float diff_time(struct timespec start, struct timespec end)
{
	return end.tv_sec - start.tv_sec +
	       0.000000001 * (end.tv_nsec - start.tv_nsec);
}

/**************************************************************************
 *
 * Function name: update_backend_status
 *        Inputs: int status, struct timespec *status_time
 *       Summary: Update hcfs_system->backend_is_online and access
 *                time.
 *                int status:
 *                    use 1 when status is online, otherwise 0.
 *                struct timespec *status_time:
 *                    time at curl request performed, set to NULL to use
 *                    current time.
 *  Return value: None (void)
 *
 *************************************************************************/
void update_backend_status(int status, struct timespec *status_time)
{
	struct timespec *last_time = &hcfs_system->backend_status_last_time;
	struct timespec current_time;

	hcfs_system->backend_is_online = status ? TRUE : FALSE;
	update_sync_state();

	if (status_time == NULL) {
		clock_gettime(CLOCK_REALTIME, &current_time);
		status_time = &current_time;
	}
	last_time->tv_sec = status_time->tv_sec;
	last_time->tv_nsec = status_time->tv_nsec;
}

inline void update_sync_state(void)
{
	if (hcfs_system->backend_is_online == FALSE ||
	    hcfs_system->sync_manual_switch == FALSE)
		hcfs_system->sync_paused = TRUE;
	else
		hcfs_system->sync_paused = FALSE;
}
