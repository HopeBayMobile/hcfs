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
extern int monitoring_interval;

class monitorTest : public ::testing::Test {
	protected:
	void SetUp() {
		hcfs_system =
		    (SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = FALSE;
	}

	void TearDown() { free(hcfs_system); }
};

TEST_F(monitorTest, Backend_Is_Online) {
	pthread_t monitor_loop_thread;
	int i;
	struct timespec req = {0, 90000000};
	int update_count = 0;
	int old_status;

	// Prepare flag to let mock hcfs_test_backend return 200
	monitoring_interval = 1;
	hcfs_test_backend_register = 200;
	hcfs_system->backend_is_online = FALSE;
	old_status = TRUE;

	// create thread to run monitor loop
	pthread_create(&monitor_loop_thread, NULL, &monitor_loop, NULL);
	for (i = 0; i < 30 ; i++) {
		if (hcfs_system->backend_is_online != old_status) {
			old_status = hcfs_system->backend_is_online;
			printf("backend_status %d\n",
			       hcfs_system->backend_is_online);
			fflush(stdout);
		}
		if (hcfs_system->backend_is_online) {
			update_count++;
			if (update_count == 1) {
				// reset first check
				puts("reset first monitor check");
				hcfs_system->backend_is_online = FALSE;
			} else {
				// expected second update after monitor interval
				break;
			}
		}
		nanosleep(&req, NULL);
	}
	// let system shut down
	hcfs_system->system_going_down = TRUE;
	// join thread
	pthread_join(monitor_loop_thread, NULL);
	ASSERT_EQ(TRUE, hcfs_system->backend_is_online);
}

TEST_F(monitorTest, Backend_Is_Offline) {
	pthread_t monitor_loop_thread;
	int i;
	struct timespec req = {0, 90000000};
	int update_count = 0;
	int old_status;

	// Prepare flag to let mock hcfs_test_backend return 401
	monitoring_interval = 1;
	hcfs_test_backend_register = 401;
	hcfs_system->backend_is_online = TRUE;
	old_status = FALSE;

	// create thread to run monitor loop
	pthread_create(&monitor_loop_thread, NULL, &monitor_loop, NULL);
	for (i = 0; i < 30 ; i++) {
		if (hcfs_system->backend_is_online != old_status) {
			old_status = hcfs_system->backend_is_online;
			printf("backend_status %d\n",
			       hcfs_system->backend_is_online);
			fflush(stdout);
		}
		if (hcfs_system->backend_is_online == FALSE) {
			update_count++;
			if (update_count == 1) {
				// reset first check
				puts("reset first monitor check");
				hcfs_system->backend_is_online = TRUE;
			} else {
				// expected second update after monitor interval
				break;
			}
		}
		nanosleep(&req, NULL);
	}
	// let system shut down
	hcfs_system->system_going_down = TRUE;
	// join thread
	pthread_join(monitor_loop_thread, NULL);
	ASSERT_EQ(FALSE, hcfs_system->backend_is_online);
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
