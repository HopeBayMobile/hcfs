#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "super_block.h"
#include "global.h"

#define NUM_THREADS_IN_POOL 4
#define NUM_CACHED_INODES 4096

typedef struct REBUILD_INODE_JOBS {
	ino_t cached_inodes[NUM_CACHED_INODES];
	int32_t num_cached_inode;
	int32_t cache_idx;
	FILE *queue_fptr;
	int64_t fptr_offset;
	pthread_mutex_t job_mutex;
	pthread_cond_t job_cond;
} REBUILD_INODE_JOBS;

typedef struct SB_THREAD {
	pthread_t tid;
	BOOL active;
	uint8_t status; /* WORKING, IDLE */
} SB_THREAD;

typedef struct SB_THREAD_POOL {
	SB_THREAD thread[NUM_THREADS_IN_POOL];
	int32_t num_active;
	int32_t num_idle;
	sem_t thpool_sem;
} SB_THREAD_POOL;

typedef struct REBUILD_SB_MGR {
	SB_THREAD_POOL thread_pool;
	REBUILD_SB_MGR jobs;
	BOOL finish; 
	sem_t mgr_sem;
} REBUILD_SB_MGR;

REBUILD_SB_MGR *rebuild_sb_mgr_info;
