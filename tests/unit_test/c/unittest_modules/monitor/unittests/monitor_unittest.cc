/* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved. */
#include <gtest/gtest.h>
#include <signal.h>
#include <errno.h>
#include <semaphore.h>
#include "mock_params.h"
extern "C" {
#include "monitor.h"
#include "global.h"
#include "fuseop.h"
#include "time.h"
}

extern SYSTEM_DATA_HEAD *hcfs_system;
extern int32_t hcfs_test_backend_register;
extern int32_t hcfs_test_backend_sleep_nsec;
extern int32_t backoff_exponent;

class monitorTest : public ::testing::Test {
	protected:
	void SetUp() {
		hcfs_system =
		    (SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = FALSE;
		hcfs_system->backend_is_online = TRUE;
		hcfs_system->sync_manual_switch = ON;
		hcfs_system->sync_paused = OFF;
		sem_init(&(hcfs_system->something_to_replace), 0, 0);
		sem_init(&(hcfs_system->monitor_sem), 1, 0);
		sem_init(&(hcfs_system->access_sem), 1, 1);

	system_config = (SYSTEM_CONF_STRUCT *)
		malloc(sizeof(SYSTEM_CONF_STRUCT));
	memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
	}

	void TearDown() {
		free(hcfs_system);
		free(system_config);
	}
};

TEST_F(monitorTest, Backend_Status_Changed) {
	pthread_t monitor_loop_thread;
	struct timespec wait_monitor_time;
	wait_monitor_time.tv_sec = 0;
	wait_monitor_time.tv_nsec = 10000000;

	// Prepare flag to let mock hcfs_test_backend return 200
	backoff_exponent = 1;
	hcfs_test_backend_register = 200;

	// create thread to run monitor loop
	pthread_create(&monitor_loop_thread, NULL, &monitor_loop, NULL);
	ASSERT_EQ(TRUE, hcfs_system->backend_is_online);

	// online -> offline
	hcfs_test_backend_register = 400;
	update_backend_status(FALSE, NULL);
	nanosleep(&wait_monitor_time, NULL);
	ASSERT_EQ(FALSE, hcfs_system->backend_is_online);

	// offline -> online
	hcfs_test_backend_register = 200;
	update_backend_status(TRUE, NULL);
	nanosleep(&wait_monitor_time, NULL);
	sem_post(&(hcfs_system->monitor_sem));
	ASSERT_EQ(TRUE, hcfs_system->backend_is_online);

	// let system shut down
	hcfs_system->system_going_down = TRUE;
	sem_post(&(hcfs_system->monitor_sem));
	// join thread
	pthread_join(monitor_loop_thread, NULL);
}

TEST_F(monitorTest, Max_Collisions_Number) {
	pthread_t monitor_loop_thread;
	struct timespec wait_monitor_time;
	wait_monitor_time.tv_sec = 0;
	wait_monitor_time.tv_nsec = 10000000;

	// Prepare flag to let mock hcfs_test_backend return 200
	hcfs_test_backend_register = 200;

	// create thread to run monitor loop
	pthread_create(&monitor_loop_thread, NULL, &monitor_loop, NULL);
	ASSERT_EQ(TRUE, hcfs_system->backend_is_online);

	hcfs_test_backend_register = 400;
	update_backend_status(FALSE, NULL);
	for (int32_t i =0; i<15;i++)
		sem_post(&(hcfs_system->monitor_sem));
	for (int32_t i =0; i<100;i++) {
		nanosleep(&wait_monitor_time, NULL);
		if(MONITOR_MAX_BACKOFF_EXPONENT == backoff_exponent)
			break;
	}
	ASSERT_EQ(MONITOR_MAX_BACKOFF_EXPONENT, backoff_exponent);

	// let system shut down
	hcfs_system->system_going_down = TRUE;
	sem_post(&(hcfs_system->monitor_sem));
	// join thread
	pthread_join(monitor_loop_thread, NULL);
}

TEST_F(monitorTest, Offline_Retransmit_Wait_Timeup) {
	// This is 
	pthread_t monitor_loop_thread;
	struct timespec wait_monitor_time;
	wait_monitor_time.tv_sec = 0;
	wait_monitor_time.tv_nsec = 1000000;

	hcfs_test_backend_register = 503;

	// create thread to run monitor loop
	sem_init(&(hcfs_system->monitor_sem), 1, 0);
	pthread_create(&monitor_loop_thread, NULL, &monitor_loop, NULL);
	update_backend_status(FALSE, NULL);
	nanosleep(&wait_monitor_time, NULL);

	// let system shut down
	hcfs_system->system_going_down = TRUE;
	sem_post(&(hcfs_system->monitor_sem));
	// join thread
	pthread_join(monitor_loop_thread, NULL);
}

TEST_F(monitorTest, Update_Backend_Status_With_Timestamp) {
	struct timespec timestamp;
	clock_gettime(CLOCK_REALTIME, &timestamp);
	hcfs_system->backend_is_online = FALSE;

	update_backend_status(TRUE, &timestamp);
	ASSERT_EQ(TRUE, hcfs_system->backend_is_online);
}

TEST_F(monitorTest, Update_Backend_Status_Without_Timestamp) {
	hcfs_system->backend_is_online = FALSE;

	update_backend_status(TRUE, NULL);
	ASSERT_EQ(TRUE, hcfs_system->backend_is_online);
}

TEST_F(monitorTest, Monitor_Log) {
	hcfs_system->backend_is_online = TRUE;
	_write_monitor_loop_status_log(0.0);
	hcfs_system->backend_is_online = FALSE;
	_write_monitor_loop_status_log(0.0);
	hcfs_system->backend_is_online = TRUE;
	_write_monitor_loop_status_log(1.2345);
	fflush(stdout);
}

TEST_F(monitorTest, destroy_monitor_loop_thread) {
	destroy_monitor_loop_thread();
}

TEST_F(monitorTest, diff_time_With_No_Endtime) {
	double test_duration = 0.0;
	struct timespec test_start;
	clock_gettime(CLOCK_REALTIME, &test_start);
	test_duration = diff_time(&test_start, NULL);
	ASSERT_GT(test_duration, 0);
}

TEST_F(monitorTest, UpdateSyncStateTest) {
	hcfs_system->backend_is_online = FALSE;
	update_sync_state();
	EXPECT_EQ(TRUE, hcfs_system->sync_paused);
	hcfs_system->backend_is_online = TRUE;
	update_sync_state();
	EXPECT_EQ(FALSE, hcfs_system->sync_paused);
	EXPECT_EQ(0, hcfs_system->systemdata.cache_replace_status);
}

TEST_F(monitorTest, UpdateSyncStateTest_FromOnlineToOffline)
{
	int32_t num_replace;

	hcfs_system->backend_is_online = FALSE;
	sem_getvalue(&(hcfs_system->something_to_replace), &num_replace);
	EXPECT_EQ(0, num_replace);

	/* Run */
	update_sync_state();

	EXPECT_EQ(TRUE, hcfs_system->sync_paused);
	sem_getvalue(&(hcfs_system->something_to_replace), &num_replace);
	EXPECT_EQ(1, num_replace);
}

TEST_F(monitorTest, UpdateSyncStateTest_FromOnlineToOffline_AlreadyNeedReplace)
{
	int32_t num_replace;

	hcfs_system->backend_is_online = FALSE;
	sem_post(&(hcfs_system->something_to_replace));
	sem_getvalue(&(hcfs_system->something_to_replace), &num_replace);
	EXPECT_EQ(1, num_replace);

	/* Run */
	update_sync_state();

	EXPECT_EQ(TRUE, hcfs_system->sync_paused);
	sem_getvalue(&(hcfs_system->something_to_replace), &num_replace);
	EXPECT_EQ(1, num_replace);
}
