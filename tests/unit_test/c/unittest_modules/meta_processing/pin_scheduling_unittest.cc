extern "C" {
#include "pin_scheduling.h"
#include "fuseop.h"
#include "global.h"
#include "super_block.h"
#include "mock_param.h"
}

#include <pthread.h>
#include "gtest/gtest.h"

extern SYSTEM_DATA_HEAD *hcfs_system;
extern SUPER_BLOCK_CONTROL *sys_super_block;

struct timespec UT_sleep {
	.tv_sec = 0,
	.tv_nsec = 99999999 /*0.1 sec sleep*/
};

class pinning_loopTest : public ::testing::Test
{
protected:
	void SetUp()
	{
		hcfs_system = (SYSTEM_DATA_HEAD *)
			malloc(sizeof(SYSTEM_DATA_HEAD));
		memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
		sys_super_block = (SUPER_BLOCK_CONTROL *)
			malloc(sizeof(SUPER_BLOCK_CONTROL));
		memset(sys_super_block, 0, sizeof(SUPER_BLOCK_CONTROL));
			
		verified_inodes_counter = 0;
		mock_inodes_counter = 0;
		memset(mock_inodes, 0, sizeof(ino_t) * TOTAL_MOCK_INODES);
		memset(verified_inodes, 0, sizeof(ino_t) * TOTAL_MOCK_INODES);
		sem_init(&verified_inodes_sem, 0, 1);
		
	}

	void TearDown()
	{
		hcfs_system->system_going_down = TRUE;
		destroy_pin_scheduler();
		free(hcfs_system);
		free(sys_super_block);
	}
};

int32_t compare (const void * a, const void * b)
{
	  return ( *(ino_t*)a - *(ino_t*)b );
}

TEST_F(pinning_loopTest, WorkNormally)
{
	/* Generate mock data */
	hcfs_system->sync_paused = FALSE;
	for (int32_t i = 0; i < TOTAL_MOCK_INODES; i++) {
		mock_inodes[i] = (i + 1) * 15;
	}
	pinning_scheduler.deep_sleep = FALSE;
	sys_super_block->head.num_pinning_inodes = TOTAL_MOCK_INODES;
	sys_super_block->head.first_pin_inode = mock_inodes[0];
	mock_inodes_counter = 1;
	
	/* Run */
	hcfs_system->system_going_down = FALSE;
	init_pin_scheduler();
	for (int32_t i = 0; i < 100; i++) {
		nanosleep(&UT_sleep, NULL);
		if (TOTAL_MOCK_INODES == verified_inodes_counter)
			break;
	}

	/* Verify */
	EXPECT_EQ(TOTAL_MOCK_INODES, verified_inodes_counter);
	qsort(verified_inodes, TOTAL_MOCK_INODES, sizeof(ino_t), compare);
	EXPECT_EQ(0, memcmp(mock_inodes, verified_inodes,
			sizeof(ino_t) * TOTAL_MOCK_INODES));
}

/*
 * Unittest for pinning_collect
 */
class pinning_collectTest : public ::testing::Test {
protected:
	void SetUp()
	{
		hcfs_system = (SYSTEM_DATA_HEAD *)
			malloc(sizeof(SYSTEM_DATA_HEAD));
		memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));

		memset(&pinning_scheduler, 0, sizeof(PINNING_SCHEDULER));
		sem_init(&(pinning_scheduler.ctl_op_sem), 0, 1);
		sem_init(&(pinning_scheduler.pinning_sem), 0,
				MAX_PINNING_FILE_CONCURRENCY);

		verified_inodes_counter = 0;
		sem_init(&verified_inodes_sem, 0, 1);
	}

	void TearDown()
	{
		free(hcfs_system);
	}
};

void* mock_thread_fctnl(void *ptr)
{
	PINNING_INFO *info = (PINNING_INFO *)ptr;

	pinning_scheduler.thread_finish[info->t_idx] = TRUE;
	return NULL;
}

TEST_F(pinning_collectTest, CollectAllTerminatedThreadsSuccess)
{
	int32_t idx, val;
	char zero[MAX_PINNING_FILE_CONCURRENCY];

	/* Create pinning collector */
	pthread_create(&pinning_scheduler.pinning_collector, NULL,
			pinning_collect, NULL);

	/* Create 100 threads */
	for (int32_t i = 0; i < 100 ; i++) {
		sem_wait(&pinning_scheduler.pinning_sem);
		sem_wait(&pinning_scheduler.ctl_op_sem);
		for (idx = 0; idx < MAX_PINNING_FILE_CONCURRENCY; idx++) {
			if (pinning_scheduler.thread_active[idx] == FALSE) {
				break;
			}
		}
		pinning_scheduler.pinning_info[idx].t_idx = idx;
		pthread_create(&pinning_scheduler.pinning_file_tid[idx], NULL,
				&mock_thread_fctnl,
				(void *)&(pinning_scheduler.pinning_info[idx]));
		pinning_scheduler.thread_active[idx] = TRUE;
		pinning_scheduler.total_active_pinning++;
		sem_post(&pinning_scheduler.ctl_op_sem);
	}

	hcfs_system->system_going_down = TRUE;
        pthread_join(pinning_scheduler.pinning_collector, NULL);
	hcfs_system->system_going_down = FALSE;

	/* Verify */
	std::cout << "Begin to Verify" << std::endl;
	EXPECT_EQ(0, pinning_scheduler.total_active_pinning);
	memset(zero, 0, sizeof(char) * MAX_PINNING_FILE_CONCURRENCY);
	EXPECT_EQ(0, memcmp(zero, pinning_scheduler.thread_active,
			sizeof(char) * MAX_PINNING_FILE_CONCURRENCY));
	sem_getvalue(&pinning_scheduler.pinning_sem, &val);
	EXPECT_EQ(MAX_PINNING_FILE_CONCURRENCY, val);
	sem_getvalue(&pinning_scheduler.ctl_op_sem, &val);
	EXPECT_EQ(1, val);
}
/*
 * End of unittest for pinning_collect
 */

/*
 * Unittest for pinning_worker()
 */
class pinning_workerTest : public ::testing::Test {
protected:
	void SetUp()
	{
		hcfs_system = (SYSTEM_DATA_HEAD *)
			malloc(sizeof(SYSTEM_DATA_HEAD));
		memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
		sys_super_block = (SUPER_BLOCK_CONTROL *)
			malloc(sizeof(SUPER_BLOCK_CONTROL));
		memset(sys_super_block, 0, sizeof(SUPER_BLOCK_CONTROL));

		memset(&pinning_scheduler, 0, sizeof(PINNING_SCHEDULER));
		sem_init(&(pinning_scheduler.ctl_op_sem), 0, 1);
		sem_init(&verified_inodes_sem, 0, 1);

		FINISH_PINNING = FALSE;
	}

	void TearDown()
	{
		free(hcfs_system);
		free(sys_super_block);
	}
};

TEST_F(pinning_workerTest, FetchBlocks_ENOSPC)
{
	PINNING_INFO pinning_info;

	pinning_info.this_inode = INO_PINNING_ENOSPC;
	pinning_info.t_idx = 0;
	pinning_worker(&pinning_info);

	EXPECT_EQ(TRUE, pinning_scheduler.deep_sleep);
	EXPECT_EQ(FALSE, FINISH_PINNING); /* In super_block_finish_pinning */
}

TEST_F(pinning_workerTest, PinningSuccess)
{
	PINNING_INFO pinning_info;

	pinning_info.this_inode = 2394732; /* Arbitrary inode*/
	pinning_info.t_idx = 0;
	pinning_worker(&pinning_info);

	EXPECT_EQ(FALSE, pinning_scheduler.deep_sleep);
	EXPECT_EQ(TRUE, FINISH_PINNING); /* In super_block_finish_pinning */
}
/*
 * End of unittest for pinning_worker()
 */
