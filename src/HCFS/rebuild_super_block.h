/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>

#include "super_block.h"
#include "global.h"

#define NUM_THREADS_IN_POOL 4
#define NUM_CACHED_INODES 4096

enum { START_REBUILD_SB, KEEP_REBUILD_SB };

enum { WORKING, IDLE }; /* Status of a worker */

typedef struct INODE_JOB_HANDLE {
	ino_t inode;
	int64_t queue_file_pos; /* position of inode number in queue file */
} INODE_JOB_HANDLE;

/* Job structure includes cached inodes structure and 
 * queue file handle. */
typedef struct CACHED_JOBS {
	ino_t cached_inodes[NUM_CACHED_INODES];
	int32_t num_cached_inode; /* # of cached inodes */
	int32_t cache_idx; /* now inode index */
} CACHED_JOBS;

typedef struct REBUILD_INODE_JOBS {
	CACHED_JOBS cache_jobs;
	sem_t queue_file_sem; /* Queue file semaphore */
	int32_t queue_fh;

	/* remaining_jobs + job_count = total jobs in queue file */
	int64_t remaining_jobs; /* # of remaining jobs */
	int64_t job_count; /* # of assigned jobs */
	BOOL job_finish; /* True if all entry had been rebuilded */ 
	pthread_mutex_t job_mutex; /* job resource lock */
	pthread_cond_t job_cond; /* job condition */
} REBUILD_SB_JOBS;

/* Thread pool structure */
typedef struct SB_THREAD {
	pthread_t tid;
	BOOL active;
	char status; /* binary status: WORKING, IDLE */
	pthread_attr_t t_attr;
} SB_THREAD;

typedef struct SB_THREAD_POOL {
	SB_THREAD thread[NUM_THREADS_IN_POOL];
	int32_t tidx[NUM_THREADS_IN_POOL]; /* thread index */
	int32_t tmaster; /* Master thread index */
	int32_t num_active; /* # of active threads */
	int32_t num_idle; /* # of conditional wait threads */
	sem_t tpool_access_sem; /* thread pool access semaphore */
} SB_THREAD_POOL;

SB_THREAD_POOL *rebuild_sb_tpool;
REBUILD_SB_JOBS *rebuild_sb_jobs;

typedef struct {
	ino_t *roots;
	int64_t num_roots;
} ROOT_INFO_T;
ROOT_INFO_T ROOT_INFO;

int32_t rebuild_super_block_entry(ino_t this_inode,
		HCFS_STAT *this_stat, char pin_status);
int32_t restore_meta_super_block_entry(ino_t this_inode,
		HCFS_STAT *ret_stat);
int32_t init_rebuild_sb(char rebuild_action);
void destroy_rebuild_sb(BOOL destroy_queue_file);
int32_t create_sb_rebuilder();
void wake_sb_rebuilder(void);
int32_t prune_this_entry(ino_t this_inode);

int32_t pull_inode_job(INODE_JOB_HANDLE *inode_job);
int32_t push_inode_job(ino_t *inode_jobs, int64_t num_inodes);
int32_t erase_inode_job(INODE_JOB_HANDLE *inode_job);

