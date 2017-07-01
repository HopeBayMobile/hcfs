/*************************************************************************
*
* Copyright © 2014-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: fuseop.h
* Abstract: The header file for FUSE definition
*
* Revision History
* 2015/2/2 Jiahong added header for this file.
* 2015/2/11 Jiahong moved some functions to hfuse_system.h.
* 2015/5/11 Jiahong modifying file meta for new block indexing / searching
* 2015/6/1 Jiahong adding structure for logger.
* 2015/6/30 Jiahong moved dir and file meta defs to other files
* 2016/5/23 Jiahong adding semaphore for cache management
* 2016/7/12 Jethro moved meta structs to meta.h
* 2017/3/28 Kewei checked system space statistics.
*
**************************************************************************/

#ifndef GW20_HCFS_FUSEOP_H_
#define GW20_HCFS_FUSEOP_H_

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <fuse/fuse_opt.h>
#include <sys/ioctl.h>

#include "FS_manager.h"
#include "global.h"
#include "meta.h"
#include "googledrive_curl.h"

extern struct fuse_lowlevel_ops hfuse_ops;

/* Hard link limit */
#define MAX_HARD_LINK 65000
#define MAX_FILE_SIZE LONG_MAX

/* Defining the names for block status */
/* Not stored on any media or storage. Value should be zero.*/
#define ST_NONE 0
/* Stored only on local cache */
#define ST_LDISK 1
/* Stored only on cloud storage */
#define ST_CLOUD 2
/* Stored both on local cache and cloud storage */
#define ST_BOTH 3
/* In transition from local cache to cloud storage */
#define ST_LtoC 4
/* In transition from cloud storage to local cache */
#define ST_CtoL 5
/* Block to be deleted in backend */
#define ST_TODELETE 6

/* Defining the names for file object types */
#define D_ISDIR 0
#define D_ISREG 1
#define D_ISLNK 2
#define D_ISFIFO 3
#define D_ISSOCK 4

/* Define constants for timestamp changes */
#define A_TIME 4
#define M_TIME 2
#define C_TIME 1

#define FUSE_SOCK_PATH "/dev/shm/fuse_communication_reporter"

/* Defines for caching value of selinux xattr */
#define SELINUX_XATTR_KEY "selinux"
#define SELINUX_EXTERNAL_XATTR_VAL "u:object_r:fuse:s0"

/* Defines for local pin property */
typedef enum {
	P_UNPIN,
	P_PIN,
	P_HIGH_PRI_PIN,
	/* end */
	NUM_PIN_TYPES,
	P_INVALID = 255,
} PIN_t;

/* Marcos to check pin property */
#define P_IS_VALID_PIN(p) (p < NUM_PIN_TYPES) /* To valid pin property */
#define P_IS_PIN(p) ((p == P_PIN) || (p == P_HIGH_PRI_PIN))
#define P_IS_UNPIN(p) (p == P_UNPIN)

#ifdef _ANDROID_ENV_
#define  IS_ANDROID_EXTERNAL(type) (((type) == (ANDROID_EXTERNAL)) || \
		((type) == (ANDROID_MULTIEXTERNAL)))
#endif

/* Defining the system meta resources */
typedef struct {
	int64_t system_size; /* data + meta (all real size) */
	int64_t system_meta_size; /* meta + sb (both block unit size) */
	int64_t super_block_size; /* sb (block unit size) */
	int64_t cache_size; /* data (local, block unit size) */
	int64_t cache_blocks;
	int64_t pinned_size; /* data(pin, block unit size) */
	int64_t unpin_dirty_data_size; /* dirty data w/ unpin property (block unit) */
	int64_t backend_size; /* data(sync) + meta(sync) */
	int64_t backend_meta_size;
	int64_t backend_inodes;
	int64_t dirty_cache_size; /* data + meta (both block unit size) */
	int64_t system_quota;
	int32_t cache_replace_status;
	/* data for xfer statistics */
	int64_t xfer_size_download;
	int64_t xfer_size_upload;
	int64_t xfer_throughput[XFER_WINDOW_MAX];
	int64_t xfer_total_obj[XFER_WINDOW_MAX];
	int32_t xfer_now_window;
} SYSTEM_DATA_TYPE;

typedef struct {
	FILE *system_val_fptr;
	SYSTEM_DATA_TYPE systemdata;
	sem_t fuse_sem;
	sem_t access_sem;
	sem_t num_cache_sleep_sem;
	sem_t check_cache_sem;
	sem_t check_next_sem;
	sem_t check_cache_replace_status_sem;
	sem_t monitor_sem;
	sem_t sync_wait_sem;
	sem_t sync_control_sem;
	sem_t pin_wait_sem;
	sem_t dsync_wait_sem;

	/* Semaphore for controlling cache management */
	sem_t something_to_replace;

	/* system state controllers */
	bool system_going_down;
	bool backend_is_online;
	bool sync_manual_switch;
	bool sync_paused;
	bool xfer_upload_in_progress;
	bool writing_sys_data;

	/* Define whether minimal apk should be used. Default is false. */
	bool use_minimal_apk;

	/* Whether we explicitly want to use minimal apk */
	bool set_minimal_apk;

	/* Root inode # of /data/app */
	ino_t data_app_root;

	/* Lots of functions will invoke download directly */
	sem_t xfer_download_in_progress_sem;
	/* Xfer window must be shifted in an interval */
	time_t last_xfer_shift_time;

	int32_t system_restoring;
	struct timespec backend_status_last_time;
} SYSTEM_DATA_HEAD;

SYSTEM_DATA_HEAD *hcfs_system;

#define NO_META_SPACE()\
	((hcfs_system->systemdata.system_meta_size > META_SPACE_LIMIT) ?\
	meta_nospc_log(__func__, __LINE__) : 0)

int32_t global_argc;
char **global_argv;
struct fuse_args global_fuse_args;

/* Functions for initializing HCFS */
void *mount_multi_thread(void *ptr);
void *mount_single_thread(void *ptr);

int32_t hook_fuse(int32_t argc, char **argv);

ino_t data_data_root;
ino_t data_smart_root;
bool mgmt_app_is_created;

/* FUSE op parameters */
#define REPLY_ATTR_TIMEOUT 0.1 /* Timeout for cached getattr results */

#define FUSE_HCFS_AVAIL_SPACE_NOTIFY	_IOW(0xff, 0x01, long)
#define WRITEBACK_CACHE_RESERVE_SPACE (1 * 1024 * 1024)

#endif  /* GW20_HCFS_FUSEOP_H_ */
