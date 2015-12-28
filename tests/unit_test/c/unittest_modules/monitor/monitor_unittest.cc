/* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved. */
#include <gtest/gtest.h>
#include <signal.h>
#include <errno.h>
extern "C" {
#include "monitor.h"
#include "global.h"
#include "fuseop.h"
#include "time.h"
}

extern SYSTEM_DATA_HEAD *hcfs_system;
extern int hcfs_test_backend_register;
extern int hcfs_test_backend_sleep_nsec;
extern int monitoring_interval;

class monitorTest : public ::testing::Test {
	protected:
	void SetUp() {
		hcfs_system =
		    (SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = FALSE;
		hcfs_system->backend_is_online = TRUE;
		hcfs_system->sync_manual_switch = ON;
		hcfs_system->sync_paused = OFF;
	}

	void TearDown() { free(hcfs_system); }
};

TEST_F(monitorTest, Backend_Is_Online) {
	pthread_t monitor_loop_thread;
	struct timespec larger_than_interval;

	// Prepare flag to let mock hcfs_test_backend return 200
	monitoring_interval = 1;
	larger_than_interval.tv_sec = monitoring_interval;
	larger_than_interval.tv_nsec = 10000000;
	hcfs_test_backend_register = 200;

	// create thread to run monitor loop
	pthread_create(&monitor_loop_thread, NULL, &monitor_loop, NULL);

	hcfs_system->backend_is_online = FALSE;
	nanosleep(&larger_than_interval, NULL);
	ASSERT_EQ(TRUE, hcfs_system->backend_is_online);

	hcfs_system->backend_is_online = FALSE;
	nanosleep(&larger_than_interval, NULL);
	ASSERT_EQ(TRUE, hcfs_system->backend_is_online);

	// let system shut down
	hcfs_system->system_going_down = TRUE;
	// join thread
	pthread_join(monitor_loop_thread, NULL);
}

TEST_F(monitorTest, Backend_Is_Offline) {
	pthread_t monitor_loop_thread;
	struct timespec larger_than_interval;

	// Prepare flag to let mock hcfs_test_backend return 401
	monitoring_interval = 1;
	larger_than_interval.tv_sec = monitoring_interval;
	larger_than_interval.tv_nsec = 10000000;
	hcfs_test_backend_register = 401;

	// create thread to run monitor loop
	pthread_create(&monitor_loop_thread, NULL, &monitor_loop, NULL);

	hcfs_system->backend_is_online = TRUE;
	nanosleep(&larger_than_interval, NULL);
	ASSERT_EQ(FALSE, hcfs_system->backend_is_online);

	hcfs_system->backend_is_online = TRUE;
	nanosleep(&larger_than_interval, NULL);
	ASSERT_EQ(FALSE, hcfs_system->backend_is_online);

	// let system shut down
	hcfs_system->system_going_down = TRUE;
	// join thread
	pthread_join(monitor_loop_thread, NULL);
}

TEST_F(monitorTest, Update_Backend_Status_With_Timestamp) {
	struct timespec timestamp;
	clock_gettime(CLOCK_REALTIME, &timestamp);

	hcfs_system->backend_status_last_time.tv_sec = 0;
	hcfs_system->backend_status_last_time.tv_nsec = 0;
	hcfs_system->backend_is_online = FALSE;

	update_backend_status(TRUE, &timestamp);
	ASSERT_EQ(TRUE, hcfs_system->backend_is_online);
	ASSERT_EQ(timestamp.tv_sec, hcfs_system->backend_status_last_time.tv_sec);
	ASSERT_EQ(timestamp.tv_nsec, hcfs_system->backend_status_last_time.tv_nsec);
}

TEST_F(monitorTest, Update_Backend_Status_Without_Timestamp) {
	hcfs_system->backend_status_last_time.tv_sec = 0;
	hcfs_system->backend_status_last_time.tv_nsec = 0;
	hcfs_system->backend_is_online = FALSE;

	update_backend_status(TRUE, NULL);
	ASSERT_EQ(TRUE, hcfs_system->backend_is_online);
	ASSERT_NE(0, hcfs_system->backend_status_last_time.tv_sec);
	ASSERT_NE(0, hcfs_system->backend_status_last_time.tv_nsec);
}

TEST_F(monitorTest, Write_Log_With_Time) {
	_write_monitor_loop_status_log(0);
	fflush(stdout);
}

TEST_F(monitorTest, Write_Log_Without_Time) {
	_write_monitor_loop_status_log(1.2345);
	fflush(stdout);
}
