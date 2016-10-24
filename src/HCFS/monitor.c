/**************************************************************************
 *
 * Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
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

#include <assert.h>
#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>

#include "fuseop.h"
#include "global.h"
#include "logger.h"
#include "hcfscurl.h"
#include "macro.h"
#include "params.h"
#include "hcfs_cacheops.h"
#include "utils.h"
#include "rebuild_super_block.h"

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
			write_log(5, "[Monitor] backend [%s] (%.2lf ms)\n",
				  backend_is_online ? "online" : "offline",
				  duration * 1000);
		else
			write_log(5, "[Monitor] backend [%s]\n",
				  backend_is_online ? "online" : "offline");
		write_log(5, "[Monitor] sync switch [%s]\n",
			  sync_manual_switch ? "on" : "off");
		write_log(5, "[Monitor] hcfs sync state [%s]\n",
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
void *monitor_loop(void *ptr)
{
	struct timespec ts;
	int32_t ret_val;
	int32_t wait_sec = 0;
	int32_t min, max;
#ifdef _ANDROID_ENV_
	prctl(PR_SET_NAME, "monitor_loop");
#endif /* _ANDROID_ENV_ */
	UNUSED(ptr);

	clock_gettime(CLOCK_MONOTONIC, &ts);
	srand( ts.tv_sec * 1000 * 1000 + ts.tv_nsec);

	monitor_curl_handle.curl_backend = NONE;
	monitor_curl_handle.curl = NULL;

	write_log(2, "[Monitor] Start monitor loop\n");

	hcfs_system->backend_is_online = check_backend_status();
	update_sync_state();

	while (hcfs_system->system_going_down == FALSE) {
		if (hcfs_system->backend_is_online == TRUE) {
			if (hcfs_system->system_restoring == RESTORING_STAGE2)
				wake_sb_rebuilder();
			sem_wait(&(hcfs_system->monitor_sem));
			continue;
		}
		if (hcfs_system->sync_manual_switch == OFF) {
			write_log(6, "Sleeping monitor thread: manual switch off\n");
			sem_wait(&(hcfs_system->monitor_sem));
			continue;
		}
		/* Change exponential backoff to fixed random interval */
		max = MONITOR_MAX_TIMEOUT;
		min = MONITOR_MIN_TIMEOUT;
		wait_sec = MONITOR_BACKOFF_SLOT;
		wait_sec *= (min + (rand() % (max - min + 1)));
		write_log(5, "[Monitor] wait %d seconds before retransmit\n",
			  wait_sec);

		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += wait_sec;
		ret_val = sem_timedwait(&(hcfs_system->monitor_sem), &ts);

		BOOL will_retry = FALSE;
		if ((ret_val == 0) && (manual_retry_conn == TRUE))
			will_retry = TRUE;
		if ((ret_val != 0) && (errno == ETIMEDOUT))
			will_retry = TRUE;

		if (will_retry == TRUE)	{
			hcfs_system->backend_is_online = check_backend_status();
			update_sync_state();
			manual_retry_conn = FALSE;
		}
	}
	if (hcfs_system->system_restoring == RESTORING_STAGE2)
		wake_sb_rebuilder();
	return NULL;
}

/**************************************************************************
 *
 * Function name: check_backend_status
 *        Inputs: none
 *       Summary: 
 *  Return value: none
 *
 *************************************************************************/
int32_t check_backend_status(void) {
	double test_duration;
	struct timespec test_stop, test_start;
	BOOL status;
	int32_t ret_val;

	now_retry_conn = TRUE; /* This flag is used to check if system is retrying conn */
	write_log(5, "[Monitor] check_backend_status\n");
	clock_gettime(CLOCK_REALTIME, &test_start);

	ret_val = hcfs_test_backend(&monitor_curl_handle);
	status = ((ret_val >= 200) && (ret_val <= 299));
	now_retry_conn = FALSE;

	clock_gettime(CLOCK_REALTIME, &test_stop);
	test_duration = diff_time(&test_start, &test_stop);
	_write_monitor_loop_status_log(test_duration);

	return status;
}


/**************************************************************************
 *
 * Function name: destroy_monitor_loop_thread
 *        Inputs: none
 *       Summary: wake monitor_loop and prepare for process
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
inline double diff_time(const struct timespec *start, const struct timespec *end)
{
	struct timespec now;

	if(end) {
		now = *end;
	} else {
		clock_gettime(CLOCK_REALTIME, &now);
	}
	return now.tv_sec - start->tv_sec +
	       0.000000001 * (now.tv_nsec - start->tv_nsec);
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
void update_backend_status(BOOL status_in, struct timespec *status_time)
{
	struct timespec current_time;
	BOOL status = !!status_in;
	BOOL status_changed = (hcfs_system->backend_is_online != status);

	hcfs_system->backend_is_online = status;
	update_sync_state();
	if (status_changed)
		sem_post(&(hcfs_system->monitor_sem));

/* TODO FIXME: status_time is not used actually */
	UNUSED(status_time);
	if (status_time == NULL) {
		clock_gettime(CLOCK_REALTIME, &current_time);
	}
}

/* Force retrying backend connection */
void force_retry_conn(void)
{
	BOOL org_status = hcfs_system->backend_is_online;

	/* Don't need to retry if backend is already online */
	if (org_status == FALSE) {
		manual_retry_conn = TRUE;
		sem_post(&(hcfs_system->monitor_sem));
	}
}

void update_sync_state(void)
{
	int32_t num_replace;
	int32_t pause_status;

	if (hcfs_system->backend_is_online == FALSE ||
	    hcfs_system->sync_manual_switch == OFF) {
		/* Change from online to offline */
		if (hcfs_system->sync_paused == FALSE) {
			hcfs_system->sync_paused = TRUE;
			/* Wake up cache manager so that it will wake
			 * all other sleeping threads up */
			sem_getvalue(&(hcfs_system->something_to_replace),
					&num_replace);
			if (num_replace == 0)
				sem_post(&(hcfs_system->something_to_replace));
		}

	} else {
		if (hcfs_system->sync_paused == TRUE) {
			sem_getvalue(&(hcfs_system->sync_wait_sem),
			             &pause_status);
			if (pause_status == 0)
				sem_post(&(hcfs_system->sync_wait_sem));
			sem_getvalue(&(hcfs_system->pin_wait_sem),
			             &pause_status);
			if (pause_status == 0)
				sem_post(&(hcfs_system->pin_wait_sem));
			sem_getvalue(&(hcfs_system->dsync_wait_sem),
			             &pause_status);
			if (pause_status == 0)
				sem_post(&(hcfs_system->dsync_wait_sem));
		}

		hcfs_system->sync_paused = FALSE;
		/* Threads can sleep on cache full now */
		sem_wait(&(hcfs_system->access_sem));
		hcfs_system->systemdata.cache_replace_status = 0;
		sem_post(&(hcfs_system->access_sem));
	}
}
