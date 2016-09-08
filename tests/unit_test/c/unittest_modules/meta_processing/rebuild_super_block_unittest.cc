#define __STDC_FORMAT_MACROS
extern "C" {
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <stdint.h>
#include <inttypes.h>
#include "rebuild_super_block.h"
#include "mock_param.h"
#include "super_block.h"
#include "global.h"
#include "fuseop.h"
#include "path_reconstruct.h"
}
#include <cstdlib>
#include "rebuild_super_block_params.h"
#include "gtest/gtest.h"

extern SYSTEM_DATA_HEAD *hcfs_system;

class superblockEnvironment : public ::testing::Environment {
	public:
		void SetUp()
		{
			system_config = (SYSTEM_CONF_STRUCT *)
				malloc(sizeof(SYSTEM_CONF_STRUCT));
			memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));

			hcfs_system = (SYSTEM_DATA_HEAD *)
					malloc(sizeof(SYSTEM_DATA_HEAD));
			memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
			sem_init(&(hcfs_system->access_sem), 0, 1);
			sem_init(&(hcfs_system->fuse_sem), 0, 0);
			if (!access("rebuild_sb_running_folder", F_OK))
				system("rm -r ./rebuild_sb_running_folder");
			mkdir("rebuild_sb_running_folder", 0700);
			METAPATH = "rebuild_sb_running_folder";
			NOW_TEST_RESTORE_META = FALSE;
		}
		void TearDown()
		{
			system("rm -r ./rebuild_sb_running_folder");
			free(hcfs_system);
			free(system_config);
		}
};

::testing::Environment* const rebuild_superblock_env =
	::testing::AddGlobalTestEnvironment(new superblockEnvironment);

/**
 * Unittest of init_rebuild_sb()
 */
class init_rebuild_sbTest: public ::testing::Test {
protected:
	char *sb_path, *queuefile_path;

	void SetUp()
	{
		sb_path = (char *) malloc(400);
		queuefile_path = (char *)malloc(400);
		sprintf(sb_path, "%s/superblock", METAPATH);
		sprintf(queuefile_path, "%s/rebuild_sb_queue", METAPATH);
		sys_super_block = (SUPER_BLOCK_CONTROL *)
				malloc(sizeof(SUPER_BLOCK_CONTROL));
		NOW_NO_ROOTS = FALSE;
		hcfs_system->system_restoring = RESTORING_STAGE2;
	}
	void TearDown()
	{
		free(sb_path);
		free(queuefile_path);
		free(sys_super_block);
		system("rm -rf ./rebuild_sb_running_folder/*");
		hcfs_system->system_restoring = NOT_RESTORING;
	}
};

TEST_F(init_rebuild_sbTest, BeginRebuildSuperBlock)
{
	SUPER_BLOCK_HEAD sb_head, exp_sb_head;
	FILE *fptr;
	ino_t exp_roots[5] = {234, 345, 456, 567, 678};
	ino_t roots[5];

	EXPECT_EQ(0, init_rebuild_sb(START_REBUILD_SB));

	/* Verify superblock */
	fptr = fopen(sb_path, "r");
	fseek(fptr, 0, SEEK_SET);
	fread(&sb_head, sizeof(SUPER_BLOCK_HEAD), 1, fptr);
	memset(&exp_sb_head, 0, sizeof(SUPER_BLOCK_HEAD));
	exp_sb_head.num_total_inodes = 5; /* Mock number is set in mock function */
	//exp_sb_head.now_rebuild = TRUE;

	EXPECT_EQ(0, memcmp(&exp_sb_head, &sb_head, sizeof(SUPER_BLOCK_HEAD)));
	fclose(fptr);
	unlink(sb_path);

	/* Verify queue file */
	pread(rebuild_sb_jobs->queue_fh, roots, sizeof(ino_t) * 5, 0);
	close(rebuild_sb_jobs->queue_fh);

	EXPECT_EQ(0, memcmp(exp_roots, roots, sizeof(ino_t) * 5));
	EXPECT_EQ(5, rebuild_sb_jobs->remaining_jobs);
	EXPECT_EQ(0, rebuild_sb_jobs->cache_jobs.cache_idx);
	EXPECT_EQ(0, rebuild_sb_jobs->cache_jobs.num_cached_inode);
	unlink(queuefile_path);

	/* Verify FSstat */
	for (int i = 0; i < 5; i++) {
		char fsstat_path[100];
		sprintf(fsstat_path, "%s/FS_sync/FSstat%"PRIu64,
			METAPATH, (uint64_t)roots[i]);
		ASSERT_EQ(0, access(fsstat_path, F_OK));
		unlink(fsstat_path);
	}

	/* Verify superblock size */
	EXPECT_EQ(sizeof(SUPER_BLOCK_HEAD) + 5 * sizeof(SUPER_BLOCK_ENTRY),
			hcfs_system->systemdata.super_block_size);

	EXPECT_EQ(RESTORING_STAGE2, hcfs_system->system_restoring);
}

TEST_F(init_rebuild_sbTest, NoRoot_DoNeedRebuild)
{
	NOW_NO_ROOTS = TRUE;

	EXPECT_EQ(-ENOENT, init_rebuild_sb(START_REBUILD_SB));
}

TEST_F(init_rebuild_sbTest, KeepRebuildSuperBlock_QueueFileExist)
{
	FILE *fptr;
	ino_t inode_in_queue[10] = {0, 0, 45, 0, 67, 78, 0, 90, 12, 5566};
	ino_t inodes[10];

	fptr = fopen(queuefile_path, "w+");
	setbuf(fptr, NULL);
	ftruncate(fileno(fptr), 0);
	fwrite(&inode_in_queue, sizeof(ino_t), 10, fptr);
	fclose(fptr);

	EXPECT_EQ(0, init_rebuild_sb(KEEP_REBUILD_SB));

	/* Verify */
	pread(rebuild_sb_jobs->queue_fh, inodes, sizeof(ino_t) * 10, 0);
	EXPECT_EQ(0, memcmp(inode_in_queue, inodes, sizeof(ino_t) * 10));
	EXPECT_EQ(10, rebuild_sb_jobs->remaining_jobs);
	EXPECT_EQ(0, rebuild_sb_jobs->cache_jobs.cache_idx);
	EXPECT_EQ(0, rebuild_sb_jobs->cache_jobs.num_cached_inode);
	close(rebuild_sb_jobs->queue_fh);
	unlink(queuefile_path);

	EXPECT_EQ(RESTORING_STAGE2, hcfs_system->system_restoring);
}

TEST_F(init_rebuild_sbTest, KeepRebuildSuperBlock_QueueFileNotExist)
{
	ino_t exp_roots[5] = {234, 345, 456, 567, 678};
	ino_t roots[5];

	EXPECT_EQ(0, init_rebuild_sb(KEEP_REBUILD_SB));

	/* Verify */
	pread(rebuild_sb_jobs->queue_fh, roots, sizeof(ino_t) * 5, 0);
	EXPECT_EQ(0, memcmp(exp_roots, roots, sizeof(ino_t) * 5));
	EXPECT_EQ(5, rebuild_sb_jobs->remaining_jobs);
	EXPECT_EQ(0, rebuild_sb_jobs->cache_jobs.cache_idx);
	EXPECT_EQ(0, rebuild_sb_jobs->cache_jobs.num_cached_inode);
	close(rebuild_sb_jobs->queue_fh);
	unlink(queuefile_path);

	EXPECT_EQ(RESTORING_STAGE2, hcfs_system->system_restoring);
}
/**
 * End unittest of init_rebuild_sb()
 */


/**
 * Unittest of destroy_rebuild_sb()
 */
class destroy_rebuild_sbTest: public ::testing::Test {
protected:
	char *sb_path, *queuefile_path;

	void SetUp()
	{
		sb_path = (char *) malloc(400);
		queuefile_path = (char *)malloc(400);
		sprintf(sb_path, "%s/superblock", METAPATH);
		sprintf(queuefile_path, "%s/rebuild_sb_queue", METAPATH);
		sys_super_block = (SUPER_BLOCK_CONTROL *)
				malloc(sizeof(SUPER_BLOCK_CONTROL));

		/* Allocate memory for jobs */
		rebuild_sb_jobs = (REBUILD_SB_JOBS *)
			calloc(sizeof(REBUILD_SB_JOBS), 1);
		memset(rebuild_sb_jobs, 0, sizeof(REBUILD_SB_JOBS));
		pthread_mutex_init(&(rebuild_sb_jobs->job_mutex), NULL);
		pthread_cond_init(&(rebuild_sb_jobs->job_cond), NULL);
		sem_init(&(rebuild_sb_jobs->queue_file_sem), 0, 1);

		/* Allocate memory for thread pool */
		rebuild_sb_tpool = (SB_THREAD_POOL *)
			calloc(sizeof(SB_THREAD_POOL), 1);
		memset(rebuild_sb_tpool, 0, sizeof(SB_THREAD_POOL));
		rebuild_sb_tpool->tmaster = -1;
		sem_init(&(rebuild_sb_tpool->tpool_access_sem), 0, 1);

		rebuild_sb_jobs->queue_fh = open(queuefile_path,
				O_CREAT | O_RDWR, 0600);
	}
	void TearDown()
	{
		free(sb_path);
		free(queuefile_path);
		free(sys_super_block);
		if (rebuild_sb_jobs)
			free(rebuild_sb_jobs);
		if (rebuild_sb_tpool)
			free(rebuild_sb_tpool);
		system("rm -rf ./rebuild_sb_running_folder/*");
	}
};

void mock_worker(void *ptr)
{
	return;
}

TEST_F(destroy_rebuild_sbTest, Destroy_RemoveQueueFile)
{
	for (int idx = 0; idx < NUM_THREADS_IN_POOL; idx++) {
		pthread_create(&(rebuild_sb_tpool->thread[idx].tid),
			NULL, (void *)&mock_worker, NULL);
	}

	destroy_rebuild_sb(TRUE);
	EXPECT_EQ(0, rebuild_sb_jobs);
	EXPECT_EQ(0, rebuild_sb_tpool);
	EXPECT_EQ(-1, access(queuefile_path, F_OK));
}

TEST_F(destroy_rebuild_sbTest, Destroy_PreserveQueueFile)
{
	for (int idx = 0; idx < NUM_THREADS_IN_POOL; idx++) {
		pthread_create(&(rebuild_sb_tpool->thread[idx].tid),
			NULL, (void *)&mock_worker, NULL);
	}

	destroy_rebuild_sb(FALSE);
	EXPECT_EQ(0, rebuild_sb_jobs);
	EXPECT_EQ(0, rebuild_sb_tpool);
	EXPECT_EQ(0, access(queuefile_path, F_OK));
	unlink(queuefile_path);
}
/**
 * End unittest of destroy_rebuild_sb()
 */

/**
 * Unittest of wake_sb_rebuilder()
 */
class wake_sb_rebuilderTest: public ::testing::Test {
protected:
	char *sb_path, *queuefile_path;

	void SetUp()
	{
		sb_path = (char *) malloc(400);
		queuefile_path = (char *)malloc(400);
		sprintf(sb_path, "%s/superblock", METAPATH);
		sprintf(queuefile_path, "%s/rebuild_sb_queue", METAPATH);
		sys_super_block = (SUPER_BLOCK_CONTROL *)
				malloc(sizeof(SUPER_BLOCK_CONTROL));

		/* Allocate memory for jobs */
		rebuild_sb_jobs = (REBUILD_SB_JOBS *)
			calloc(sizeof(REBUILD_SB_JOBS), 1);
		memset(rebuild_sb_jobs, 0, sizeof(REBUILD_SB_JOBS));
		pthread_mutex_init(&(rebuild_sb_jobs->job_mutex), NULL);
		pthread_cond_init(&(rebuild_sb_jobs->job_cond), NULL);
		sem_init(&(rebuild_sb_jobs->queue_file_sem), 0, 1);

		/* Allocate memory for thread pool */
		rebuild_sb_tpool = (SB_THREAD_POOL *)
			calloc(sizeof(SB_THREAD_POOL), 1);
		memset(rebuild_sb_tpool, 0, sizeof(SB_THREAD_POOL));
		rebuild_sb_tpool->tmaster = -1;
		sem_init(&(rebuild_sb_tpool->tpool_access_sem), 0, 1);
	}
	void TearDown()
	{
		free(sb_path);
		free(queuefile_path);
		free(sys_super_block);
		if (rebuild_sb_jobs)
			free(rebuild_sb_jobs);
		if (rebuild_sb_tpool)
			free(rebuild_sb_tpool);
		system("rm -rf ./rebuild_sb_running_folder/*");
	}
};

void mock_worker2(void *ptr)
{
	int idx = *(int *)ptr;

	pthread_mutex_lock(&(rebuild_sb_jobs->job_mutex));
	printf("thread is going to wait\n");

	sem_wait(&(rebuild_sb_tpool->tpool_access_sem));
	rebuild_sb_tpool->num_idle++;
	sem_post(&(rebuild_sb_tpool->tpool_access_sem));

	pthread_cond_broadcast(&(rebuild_sb_jobs->job_cond));
	rebuild_sb_tpool->num_idle--;
	rebuild_sb_tpool->thread[idx].active = FALSE;
	pthread_mutex_unlock(&(rebuild_sb_jobs->job_mutex));
	return;
}

TEST_F(wake_sb_rebuilderTest, WakeUpAllThreads)
{
	struct timespec sleep_time;

	sleep_time.tv_sec = 0;
	sleep_time.tv_nsec = 100000000;

	hcfs_system->system_restoring = TRUE;
	rebuild_sb_tpool->num_idle = 0;
	rebuild_sb_jobs->job_finish = FALSE;

	for (int idx = 0; idx < NUM_THREADS_IN_POOL; idx++) {
		rebuild_sb_tpool->tidx[idx] = idx;
		pthread_create(&(rebuild_sb_tpool->thread[idx].tid),
			NULL, (void *)&mock_worker2,
			&(rebuild_sb_tpool->tidx[idx]));
		rebuild_sb_tpool->thread[idx].active = TRUE;
	}

	nanosleep(&sleep_time, NULL);
	wake_sb_rebuilder();

	/* Verify */
	for (int idx = 0; idx < NUM_THREADS_IN_POOL; idx++) {
		EXPECT_EQ(FALSE, rebuild_sb_tpool->thread[idx].active);
		pthread_join(rebuild_sb_tpool->thread[idx].tid, NULL);
	}

}
/**
 * End unittest of wake_sb_rebuilder()
 */

/**
 * Unittest of push_inode_job()
 */
class push_inode_jobTest: public ::testing::Test {
protected:
	char *sb_path, *queuefile_path;

	void SetUp()
	{
		sb_path = (char *) malloc(400);
		queuefile_path = (char *)malloc(400);
		sprintf(sb_path, "%s/superblock", METAPATH);
		sprintf(queuefile_path, "%s/rebuild_sb_queue", METAPATH);
		sys_super_block = (SUPER_BLOCK_CONTROL *)
				malloc(sizeof(SUPER_BLOCK_CONTROL));

		/* Allocate memory for jobs */
		rebuild_sb_jobs = (REBUILD_SB_JOBS *)
			calloc(sizeof(REBUILD_SB_JOBS), 1);
		memset(rebuild_sb_jobs, 0, sizeof(REBUILD_SB_JOBS));
		pthread_mutex_init(&(rebuild_sb_jobs->job_mutex), NULL);
		pthread_cond_init(&(rebuild_sb_jobs->job_cond), NULL);
		sem_init(&(rebuild_sb_jobs->queue_file_sem), 0, 1);

		/* Allocate memory for thread pool */
		rebuild_sb_tpool = (SB_THREAD_POOL *)
			calloc(sizeof(SB_THREAD_POOL), 1);
		memset(rebuild_sb_tpool, 0, sizeof(SB_THREAD_POOL));
		rebuild_sb_tpool->tmaster = -1;
		sem_init(&(rebuild_sb_tpool->tpool_access_sem), 0, 1);

		rebuild_sb_jobs->queue_fh = open(queuefile_path,
				O_CREAT | O_RDWR, 0600);
	}
	void TearDown()
	{
		free(sb_path);
		free(queuefile_path);
		free(sys_super_block);
		if (rebuild_sb_jobs)
			free(rebuild_sb_jobs);
		if (rebuild_sb_tpool)
			free(rebuild_sb_tpool);
		unlink(queuefile_path);
		system("rm -rf ./rebuild_sb_running_folder/*");
	}
};

TEST_F(push_inode_jobTest, PushInodesInto_EmptyQueue)
{
	ino_t inodes[5] = {123,234,345,456,567};
	ino_t verified_inodes[5] = {0};

	EXPECT_EQ(0, push_inode_job(inodes, 5));

	/* Verify */
	EXPECT_EQ(5, rebuild_sb_jobs->remaining_jobs);
	EXPECT_EQ(0, rebuild_sb_jobs->job_count);
	EXPECT_EQ(0, rebuild_sb_jobs->cache_jobs.num_cached_inode);
	EXPECT_EQ(0, rebuild_sb_jobs->cache_jobs.cache_idx);
	pread(rebuild_sb_jobs->queue_fh, &verified_inodes,
			sizeof(ino_t) * 5, 0);
	EXPECT_EQ(0, memcmp(inodes, verified_inodes, sizeof(ino_t) * 5));

	close(rebuild_sb_jobs->queue_fh);
}

TEST_F(push_inode_jobTest, PushInodesInto_NonemptyQueue)
{
	struct stat tmpstat;
	ino_t inodes[5] = {123,234,345,456,567};
	ino_t verified_inodes[150] = {0};
	ino_t inodes_in_file[150];

	/* Mock data */
	for (int i = 0; i < 150; i++)
		inodes_in_file[i] = i;
	pwrite(rebuild_sb_jobs->queue_fh, inodes_in_file,
		sizeof(ino_t) * 150, 0);
	rebuild_sb_jobs->remaining_jobs = 100;
	rebuild_sb_jobs->job_count = 50;

	EXPECT_EQ(0, push_inode_job(inodes, 5));

	/* Verify */
	EXPECT_EQ(105, rebuild_sb_jobs->remaining_jobs);
	EXPECT_EQ(50, rebuild_sb_jobs->job_count);
	EXPECT_EQ(0, rebuild_sb_jobs->cache_jobs.num_cached_inode);
	EXPECT_EQ(0, rebuild_sb_jobs->cache_jobs.cache_idx);
	pread(rebuild_sb_jobs->queue_fh, &verified_inodes,
			sizeof(ino_t) * 150, 0);
	EXPECT_EQ(0, memcmp(inodes_in_file, verified_inodes,
			sizeof(ino_t) * 150));
	pread(rebuild_sb_jobs->queue_fh, &verified_inodes,
			sizeof(ino_t) * 5, sizeof(ino_t) * 150);
	EXPECT_EQ(0, memcmp(inodes, verified_inodes, sizeof(ino_t) * 5));

	fstat(rebuild_sb_jobs->queue_fh, &tmpstat);
	EXPECT_EQ(sizeof(ino_t) * 155, tmpstat.st_size);
	close(rebuild_sb_jobs->queue_fh);
}

/**
 * End of unittest of push_inode_job()
 */

/**
 * Unittest of pull_inode_job()
 */
class pull_inode_jobTest: public ::testing::Test {
protected:
	char *sb_path, *queuefile_path;

	void SetUp()
	{
		sb_path = (char *) malloc(400);
		queuefile_path = (char *)malloc(400);
		sprintf(sb_path, "%s/superblock", METAPATH);
		sprintf(queuefile_path, "%s/rebuild_sb_queue", METAPATH);
		sys_super_block = (SUPER_BLOCK_CONTROL *)
				malloc(sizeof(SUPER_BLOCK_CONTROL));

		/* Allocate memory for jobs */
		rebuild_sb_jobs = (REBUILD_SB_JOBS *)
			calloc(sizeof(REBUILD_SB_JOBS), 1);
		memset(rebuild_sb_jobs, 0, sizeof(REBUILD_SB_JOBS));
		pthread_mutex_init(&(rebuild_sb_jobs->job_mutex), NULL);
		pthread_cond_init(&(rebuild_sb_jobs->job_cond), NULL);
		sem_init(&(rebuild_sb_jobs->queue_file_sem), 0, 1);

		/* Allocate memory for thread pool */
		rebuild_sb_tpool = (SB_THREAD_POOL *)
			calloc(sizeof(SB_THREAD_POOL), 1);
		memset(rebuild_sb_tpool, 0, sizeof(SB_THREAD_POOL));
		rebuild_sb_tpool->tmaster = -1;
		sem_init(&(rebuild_sb_tpool->tpool_access_sem), 0, 1);

		rebuild_sb_jobs->queue_fh = open(queuefile_path,
				O_CREAT | O_RDWR, 0600);
	}
	void TearDown()
	{
		free(sb_path);
		free(queuefile_path);
		free(sys_super_block);
		if (rebuild_sb_jobs)
			free(rebuild_sb_jobs);
		if (rebuild_sb_tpool)
			free(rebuild_sb_tpool);
		unlink(queuefile_path);
		system("rm -rf ./rebuild_sb_running_folder/*");
	}
};

TEST_F(pull_inode_jobTest, PullExistJobManyTimes)
{
	int num_inodes = 523456;
	ino_t many_inodes[num_inodes];
	INODE_JOB_HANDLE job_handle;

	for (int i = 0; i < num_inodes; i++) {
		many_inodes[i] = i * 3 + 1;
	}
	pwrite(rebuild_sb_jobs->queue_fh, many_inodes,
		sizeof(ino_t) * num_inodes, 0);
	rebuild_sb_jobs->remaining_jobs = num_inodes;

	/* Run */
	for (int i = 0; i < num_inodes; i++) {
		ASSERT_EQ(0, pull_inode_job(&job_handle));
		ASSERT_EQ(many_inodes[i], job_handle.inode);
		ASSERT_EQ(i * sizeof(ino_t), job_handle.queue_file_pos);
		ASSERT_EQ(num_inodes - i - 1, rebuild_sb_jobs->remaining_jobs);
		ASSERT_EQ(i + 1, rebuild_sb_jobs->job_count);
		ASSERT_EQ(i % NUM_CACHED_INODES + 1,
				rebuild_sb_jobs->cache_jobs.cache_idx);
		if (i / NUM_CACHED_INODES < num_inodes / NUM_CACHED_INODES)
			ASSERT_EQ(NUM_CACHED_INODES,
				rebuild_sb_jobs->cache_jobs.num_cached_inode);
		else
			ASSERT_EQ(num_inodes % NUM_CACHED_INODES,
				rebuild_sb_jobs->cache_jobs.num_cached_inode);
	}

	ASSERT_EQ(-ENOENT, pull_inode_job(&job_handle));
	close(rebuild_sb_jobs->queue_fh);
}

TEST_F(pull_inode_jobTest, PullJob_ManyEmptyInode)
{
	int num_inodes = 523456;
	ino_t many_inodes[num_inodes];
	INODE_JOB_HANDLE job_handle;

	for (int i = 0; i < num_inodes; i++) {
		if (i % 2)
			many_inodes[i] = i * 3 + 1;
		else
			many_inodes[i] = 0; /* Skip inode 0 */
	}
	pwrite(rebuild_sb_jobs->queue_fh, many_inodes,
		sizeof(ino_t) * num_inodes, 0);
	rebuild_sb_jobs->remaining_jobs = num_inodes;

	/* Run */
	for (int i = 0; i < num_inodes; i++) {
		if (!(i % 2)) /* inode zero will be skipped */
			continue;

		ASSERT_EQ(0, pull_inode_job(&job_handle));
		ASSERT_EQ(many_inodes[i], job_handle.inode);
		ASSERT_EQ(i * sizeof(ino_t), job_handle.queue_file_pos);
		ASSERT_EQ(num_inodes - i - 1, rebuild_sb_jobs->remaining_jobs);
		ASSERT_EQ(i + 1, rebuild_sb_jobs->job_count);
		ASSERT_EQ(i % NUM_CACHED_INODES + 1,
				rebuild_sb_jobs->cache_jobs.cache_idx);
		if (i / NUM_CACHED_INODES < num_inodes / NUM_CACHED_INODES)
			ASSERT_EQ(NUM_CACHED_INODES,
				rebuild_sb_jobs->cache_jobs.num_cached_inode);
		else
			ASSERT_EQ(num_inodes % NUM_CACHED_INODES,
				rebuild_sb_jobs->cache_jobs.num_cached_inode);
	}

	ASSERT_EQ(-ENOENT, pull_inode_job(&job_handle));
	close(rebuild_sb_jobs->queue_fh);
}

TEST_F(pull_inode_jobTest, QueueFileEmpty_But_RemainingJobNotZero)
{
	INODE_JOB_HANDLE job_handle;

	rebuild_sb_jobs->remaining_jobs = 123;

	EXPECT_EQ(-ENOENT, pull_inode_job(&job_handle));
	EXPECT_EQ(0, rebuild_sb_jobs->cache_jobs.cache_idx);
	EXPECT_EQ(0, rebuild_sb_jobs->cache_jobs.num_cached_inode);
	EXPECT_EQ(0, rebuild_sb_jobs->remaining_jobs);

	close(rebuild_sb_jobs->queue_fh);
}
/**
 * End of unittest of pull_inode_job()
 */

/**
 * Unittest of erase_inode_job()
 */
class erase_inode_jobTest: public ::testing::Test {
protected:
	char *sb_path, *queuefile_path;

	void SetUp()
	{
		sb_path = (char *) malloc(400);
		queuefile_path = (char *)malloc(400);
		sprintf(sb_path, "%s/superblock", METAPATH);
		sprintf(queuefile_path, "%s/rebuild_sb_queue", METAPATH);
		sys_super_block = (SUPER_BLOCK_CONTROL *)
				malloc(sizeof(SUPER_BLOCK_CONTROL));

		/* Allocate memory for jobs */
		rebuild_sb_jobs = (REBUILD_SB_JOBS *)
			calloc(sizeof(REBUILD_SB_JOBS), 1);
		memset(rebuild_sb_jobs, 0, sizeof(REBUILD_SB_JOBS));
		pthread_mutex_init(&(rebuild_sb_jobs->job_mutex), NULL);
		pthread_cond_init(&(rebuild_sb_jobs->job_cond), NULL);
		sem_init(&(rebuild_sb_jobs->queue_file_sem), 0, 1);

		rebuild_sb_jobs->queue_fh = open(queuefile_path,
				O_CREAT | O_RDWR, 0600);
	}
	void TearDown()
	{
		free(sb_path);
		free(queuefile_path);
		free(sys_super_block);
		if (rebuild_sb_jobs)
			free(rebuild_sb_jobs);
		unlink(queuefile_path);
		system("rm -rf ./rebuild_sb_running_folder/*");
	}
};

TEST_F(erase_inode_jobTest, EraseInodeSuccess)
{
	INODE_JOB_HANDLE job_handle;
	ino_t inodes[5] = {123,234,345,456,567};
	ino_t zero_inodes[5] = {0}, verified_inodes[5];

	pwrite(rebuild_sb_jobs->queue_fh, inodes,
			sizeof(ino_t) * 5, 0);

	/* Run */
	for (int i = 0; i < 5; i++) {
		job_handle.inode = inodes[i];
		job_handle.queue_file_pos = sizeof(ino_t) * i;
		ASSERT_EQ(0, erase_inode_job(&job_handle));
	}

	memset(zero_inodes, 0, sizeof(ino_t) * 5);
	pread(rebuild_sb_jobs->queue_fh, verified_inodes,
			sizeof(ino_t) * 5, 0);
	EXPECT_EQ(0, memcmp(zero_inodes, verified_inodes, sizeof(ino_t) * 5));

	close(rebuild_sb_jobs->queue_fh);
}
/**
 * End unittest of erase_inode_job()
 */

/**
 * Unittest of create_sb_rebuilder()
 */
class create_sb_rebuilderTest: public ::testing::Test {
protected:
	char *sb_path, *queuefile_path;

	void SetUp()
	{
		sb_path = (char *) malloc(400);
		queuefile_path = (char *)malloc(400);
		sprintf(sb_path, "%s/superblock", METAPATH);
		sprintf(queuefile_path, "%s/rebuild_sb_queue", METAPATH);
		sys_super_block = (SUPER_BLOCK_CONTROL *)
				malloc(sizeof(SUPER_BLOCK_CONTROL));

		/* Allocate memory for jobs */
		rebuild_sb_jobs = (REBUILD_SB_JOBS *)
			calloc(sizeof(REBUILD_SB_JOBS), 1);
		memset(rebuild_sb_jobs, 0, sizeof(REBUILD_SB_JOBS));
		pthread_mutex_init(&(rebuild_sb_jobs->job_mutex), NULL);
		pthread_cond_init(&(rebuild_sb_jobs->job_cond), NULL);
		sem_init(&(rebuild_sb_jobs->queue_file_sem), 0, 1);

		/* Allocate memory for thread pool */
		rebuild_sb_tpool = (SB_THREAD_POOL *)
			calloc(sizeof(SB_THREAD_POOL), 1);
		memset(rebuild_sb_tpool, 0, sizeof(SB_THREAD_POOL));
		rebuild_sb_tpool->tmaster = -1;
		sem_init(&(rebuild_sb_tpool->tpool_access_sem), 0, 1);

		rebuild_sb_jobs->queue_fh = open(queuefile_path,
				O_CREAT | O_RDWR, 0600);

		memset(record_inode, 0, 100000);
		sem_init(&record_inode_sem, 0, 1);
		max_record_inode = 0;
	}
	void TearDown()
	{
		free(sb_path);
		free(queuefile_path);
		free(sys_super_block);
		if (rebuild_sb_jobs)
			free(rebuild_sb_jobs);
		if (rebuild_sb_tpool)
			free(rebuild_sb_tpool);
		unlink(queuefile_path);
		system("rm -rf ./rebuild_sb_running_folder/*");
		hcfs_system->system_restoring = NOT_RESTORING;
	}
};

TEST_F(create_sb_rebuilderTest, NoBackendInfo)
{
	CURRENT_BACKEND = NONE;

	EXPECT_EQ(-EPERM, create_sb_rebuilder());
	close(rebuild_sb_jobs->queue_fh);
}

TEST_F(create_sb_rebuilderTest, EmptyQueueFile_RebuildSuccess)
{
	int32_t val;

	CURRENT_BACKEND = SWIFT;

	hcfs_system->system_going_down = FALSE;
	hcfs_system->backend_is_online = TRUE;
	hcfs_system->system_restoring = RESTORING_STAGE2;
	//sys_super_block->head.now_rebuild = TRUE;
	EXPECT_EQ(0, create_sb_rebuilder());

	/* Wait */
	sem_wait(&(hcfs_system->fuse_sem));
	sleep(1);

	/* Verify */
	EXPECT_EQ(0, rebuild_sb_tpool->num_active);
	for (int i = 0; i < NUM_THREADS_IN_POOL; i++)
		EXPECT_EQ(FALSE, rebuild_sb_tpool->thread[i].active);
	//EXPECT_EQ(FALSE, sys_super_block->head.now_rebuild);
	destroy_rebuild_sb(FALSE);
}

TEST_F(create_sb_rebuilderTest, RootInQueueFile_BackendFromOfflineToOnline)
{
	ino_t roots[3] = {1, 2, 3};
	struct timespec sleep_time;
	ino_t cache[4096], zero[4096];
	size_t ret_size;
	FILE *fptr;

	sleep_time.tv_sec = 0;
	sleep_time.tv_nsec = 100000000;

	pwrite(rebuild_sb_jobs->queue_fh, roots, sizeof(ino_t) * 3, 0);
	rebuild_sb_jobs->remaining_jobs = 3;

	CURRENT_BACKEND = SWIFT;

	hcfs_system->system_going_down = FALSE;
	hcfs_system->backend_is_online = FALSE;
	hcfs_system->system_restoring = RESTORING_STAGE2;
	//sys_super_block->head.now_rebuild = TRUE;
	EXPECT_EQ(0, create_sb_rebuilder());

	nanosleep(&sleep_time, NULL);
	hcfs_system->backend_is_online = TRUE;
	wake_sb_rebuilder();

	/* Wait */
	sem_wait(&(hcfs_system->fuse_sem));
	sleep(1);

	/* Verify */
	EXPECT_EQ(0, rebuild_sb_tpool->num_active);
	for (int i = 0; i < NUM_THREADS_IN_POOL; i++)
		EXPECT_EQ(FALSE, rebuild_sb_tpool->thread[i].active);
	//EXPECT_EQ(FALSE, sys_super_block->head.now_rebuild);
	destroy_rebuild_sb(FALSE);

	for (int i = 1; i <= max_record_inode ; i++)
		ASSERT_EQ(TRUE, record_inode[i]);

	memset(zero, 0, sizeof(ino_t) * 4096);
	fptr = fopen(queuefile_path, "r");
	fseek(fptr, 0, SEEK_SET);
	while (!feof(fptr)) {
		ret_size = fread(cache, sizeof(ino_t), 4096, fptr);
		ASSERT_EQ(0, memcmp(zero, cache, sizeof(ino_t) * ret_size));
	}
	fclose(fptr);
}
/**
 * End unittest of create_sb_rebuilder()
 */

/**
 * Unittest of rebuild_super_block_entry()
 */
class restore_meta_super_block_entryTest: public ::testing::Test {
protected:
	char *sb_path, *queuefile_path;

	void SetUp()
	{
		sb_path = (char *) malloc(400);
		sprintf(sb_path, "%s/superblock", METAPATH);
		sys_super_block = (SUPER_BLOCK_CONTROL *)
				malloc(sizeof(SUPER_BLOCK_CONTROL));
		sys_super_block->iofptr = open(sb_path, O_CREAT | O_RDWR, 0600);
		NOW_TEST_RESTORE_META = TRUE;
		NO_PARENTS = TRUE;
		sem_init(&pathlookup_data_lock, 0, 1);
	}
	void TearDown()
	{
		NOW_TEST_RESTORE_META = FALSE;
		NO_PARENTS = FALSE;
		close(sys_super_block->iofptr);
		free(sb_path);
		free(sys_super_block);
		unlink(sb_path);
		system("rm -rf ./rebuild_sb_running_folder/*");
		sem_destroy(&pathlookup_data_lock);
	}
};

TEST_F(restore_meta_super_block_entryTest, RestoreSuccess)
{
	ino_t inode;
	HCFS_STAT tmpstat, teststat;
	SUPER_BLOCK_ENTRY exp_entry, test_entry;

	inode = 15;
	memset(&exp_stat, 0, sizeof(HCFS_STAT));
	memset(&teststat, 0, sizeof(HCFS_STAT));
	memset(&exp_filemeta, 0, sizeof(FILE_META_TYPE));
	exp_stat.st_mode = S_IFREG;
	exp_stat.st_size = 5566;
	exp_filemeta.local_pin = P_PIN;

	EXPECT_EQ(0, restore_meta_super_block_entry(inode, &tmpstat));

	teststat.mode = tmpstat.mode;
	teststat.size = tmpstat.size;
	/* Verify */
	EXPECT_EQ(0, memcmp(&exp_stat, &tmpstat, sizeof(HCFS_STAT)));

	memset(&exp_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	memcpy(&(exp_entry.inode_stat), &exp_stat, sizeof(HCFS_STAT));
	exp_entry.this_index = inode;
	exp_entry.generation = 1;
	exp_entry.status = NO_LL;
	exp_entry.pin_status = ST_PINNING;
	pread(sys_super_block->iofptr, &test_entry, sizeof(SUPER_BLOCK_ENTRY),
			sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY));
	EXPECT_EQ(0, memcmp(&exp_entry, &test_entry,
				sizeof(SUPER_BLOCK_ENTRY)));
}

TEST_F(restore_meta_super_block_entryTest, RestoreNoEntry_ModifySuperBlockStatus)
{
	ino_t inode;
	SUPER_BLOCK_ENTRY exp_entry, test_entry;

	/* Set flag */
	RESTORED_META_NOT_FOUND = TRUE;

	inode = 15;
	memset(&exp_filemeta, 0, sizeof(FILE_META_TYPE));
	exp_filemeta.local_pin = P_PIN;

	/* Run */
	EXPECT_EQ(-ENOENT, restore_meta_super_block_entry(inode, NULL));

	/* Verify */
	memset(&exp_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	exp_entry.status = TO_BE_RECLAIMED;
	pread(sys_super_block->iofptr, &test_entry, sizeof(SUPER_BLOCK_ENTRY),
			sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY));
	EXPECT_EQ(0, memcmp(&exp_entry, &test_entry,
				sizeof(SUPER_BLOCK_ENTRY)));

	/* Recovery */
	RESTORED_META_NOT_FOUND = TRUE;
}

/**
 * End unittest of rebuild_super_block_entry()
 */

/**
 * Unittest of prune_this_entry()
 */
class prune_this_entryTest: public ::testing::Test {
protected:

	void SetUp()
	{
		NO_PARENTS = FALSE;
		sem_init(&pathlookup_data_lock, 0, 1);
		remove_count = 0;
	}

	void TearDown()
	{
		for (int i = 0; i < remove_count; i++) {
			free(remove_list[i]);
			remove_list[i] = NULL;
		}
		sem_destroy(&pathlookup_data_lock);
		remove_count = 0;
	}
};

TEST_F(prune_this_entryTest, NoParents)
{
	ino_t this_inode = 10;

	NO_PARENTS = TRUE;
	EXPECT_EQ(0, prune_this_entry(this_inode));

	NO_PARENTS = FALSE;
}

TEST_F(prune_this_entryTest, RemoveEntrySuccess)
{
	ino_t this_inode = 10;
	char exp_name[200];

	EXPECT_EQ(0, prune_this_entry(this_inode));

	for (int i = 0; i < remove_count; i++) {
		/* Entry name are recorded in dir_remove_entry() */
		sprintf(exp_name, "test%d", i * 2 + 1);
		ASSERT_EQ(exp_name, remove_list[i]);
	}
}

/**
 * End of unittest of prune_this_entry()
 */
