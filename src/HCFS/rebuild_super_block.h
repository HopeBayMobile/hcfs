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

enum { START_REBUILD_SB, KEEP_REBUILD_SB };

enum { WORKING, IDLE }; /* Status of a worker */

typedef struct INODE_JOB_HANDLE {
	ino_t inode;
	int64_t queue_file_pos;
} INODE_JOB_HANDLE;

typedef struct CACHED_JOBS {
	ino_t cached_inodes[NUM_CACHED_INODES];
	int32_t num_cached_inode;
	int32_t cache_idx;
} CACHED_JOBS;

typedef struct REBUILD_INODE_JOBS {
	CACHED_JOBS cache_jobs;
	int32_t queue_fh;
	int64_t remaining_jobs;
	int64_t job_count;
	BOOL job_finish; 
	pthread_mutex_t job_mutex;
	pthread_cond_t job_cond;
} REBUILD_SB_JOBS;

typedef struct SB_THREAD {
	pthread_t tid;
	BOOL active;
	char status; /* WORKING, IDLE */
	pthread_attr_t t_attr;
} SB_THREAD;

typedef struct SB_THREAD_POOL {
	SB_THREAD thread[NUM_THREADS_IN_POOL];
	int32_t tidx[NUM_THREADS_IN_POOL];
	int32_t tmaster;
	int32_t num_active;
	int32_t num_idle;
	sem_t tpool_access_sem;
} SB_THREAD_POOL;

//typedef struct REBUILD_SB_MGR {
//	SB_THREAD_POOL tpool;
//	sem_t mgr_access_sem;
	//pthread_attr_t mgr_attr;
	//pthread_t mgr_tid;
//} REBUILD_SB_MGR_INFO;

SB_THREAD_POOL *rebuild_sb_tpool;
REBUILD_SB_JOBS *rebuild_sb_jobs;

int32_t rebuild_super_block_entry(ino_t this_inode,
		struct stat *this_stat, char pin_status);
int32_t restore_meta_super_block_entry(ino_t this_inode,
		struct stat *ret_stat);
int32_t init_rebuild_sb(char rebuild_action);
void destroy_rebuild_sb(void);
int32_t create_sb_rebuilder();
void wake_sb_rebuilder(void);
