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

void _write_monitor_loop_status_log(double duration)
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
		if (duration != 0)
			write_log(10, "[Monitor] backend [%s] (%.2lf ms)\n",
				  backend_is_online ? "online" : "offline",
				  duration * 1000);
		else
			write_log(10, "[Monitor] backend [%s]\n",
				  backend_is_online ? "online" : "offline");
		write_log(10, "[Monitor] sync switch [%s]\n",
			  sync_manual_switch ? "on" : "off");
		write_log(10, "[Monitor] hcfs sync state [%s]\n",
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
	struct timespec test_stop, test_start;
	double test_duration;
	double idle_time;
	int32_t ret_val;
	BOOL status;
#ifdef _ANDROID_ENV_
	UNUSED(ptr);
	prctl(PR_SET_NAME, "monitor_loop");
#endif /* _ANDROID_ENV_ */

	hcfs_system->backend_status_last_time.tv_sec = 0;
	hcfs_system->backend_status_last_time.tv_nsec = 0;
	monitor_curl_handle.curl_backend = NONE;
	monitor_curl_handle.curl = NULL;

	write_log(2, "[Monitor] Start monitor loop\n");

	while (hcfs_system->system_going_down == FALSE) {
		if (hcfs_system->monitor_interval == 0) {
			sem_wait(&(hcfs_system->monitor_sem));
			/* sem_post with monitor_interval == 0
			 * can trigger single test */
		}

		test_duration = 0.0;
		idle_time =
		    diff_time(&hcfs_system->backend_status_last_time, NULL);
		if (idle_time >= hcfs_system->monitor_interval) {
			clock_gettime(CLOCK_REALTIME, &test_start);
			write_log(10, "[Monitor] check_backend_status\n");
			ret_val = hcfs_test_backend(&monitor_curl_handle);
			status = ((ret_val >= 200) && (ret_val <= 299));
			clock_gettime(CLOCK_REALTIME, &test_stop);
			test_duration = diff_time(&test_start, &test_stop);

			update_backend_status(status, &test_stop);
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
 * Function name: destroy_monitor_loop_thread
 *        Inputs: none
 *       Summary: awake monitor_loop and prepaire for process
 *       termination, require hcfs_system->system_going_down == FALSE,
 *       otherwise loop will wait again immediately.
 *  Return value: none
 *
 *************************************************************************/
void destroy_monitor_loop_thread(void)
{
	sem_post(&(hcfs_system->monitor_sem));
}
/**************************************************************************
 *
 * Function name: diff_time
 *        Inputs: timespec start
 *                timespec end
 *       Summary: Calculate time duration between [start] and [end]
 *  Return value: double, duration between [start] and [end]
 *
 *************************************************************************/
inline double diff_time(const struct timespec *start, struct timespec *end)
{
	struct timespec now;

	if (end == NULL) {
		clock_gettime(CLOCK_REALTIME, &now);
		end = &now;
	}
	return end->tv_sec - start->tv_sec +
	       0.000000001 * (end->tv_nsec - start->tv_nsec);
}

/**************************************************************************
 *
 * Function name: update_backend_status
 *        Inputs: BOOL status, struct timespec *status_time
 *       Summary: Update hcfs_system->backend_is_online and access
 *                time.
 *                BOOL status:
 *                    use 1 when status is online, otherwise 0.
 *                struct timespec *status_time:
 *                    time at curl request performed, set to NULL to use
 *                    current time.
 *  Return value: None (void)
 *
 *************************************************************************/
void update_backend_status(register BOOL status, struct timespec *status_time)
{
	struct timespec current_time;

	hcfs_system->backend_is_online = !!status;
	update_sync_state();

	if (status_time == NULL) {
		clock_gettime(CLOCK_REALTIME, &current_time);
		status_time = &current_time;
	}
	hcfs_system->backend_status_last_time.tv_sec = status_time->tv_sec;
	hcfs_system->backend_status_last_time.tv_nsec = status_time->tv_nsec;
}

void update_sync_state(void)
{
	if (hcfs_system->backend_is_online == FALSE ||
	    hcfs_system->sync_manual_switch == FALSE)
		hcfs_system->sync_paused = TRUE;
	else
		hcfs_system->sync_paused = FALSE;
}
