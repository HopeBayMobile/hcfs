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

#include "global.h"
#include "params.h"
#include "dedup_table.h"

extern struct fuse_lowlevel_ops hfuse_ops;

/*BEGIN META definition*/

/* Defining parameters for B-tree operations (dir entries). */
/* Max number of children per node is 100, min is 50, so at least 49 elements
*  in each node (except the root) */
#define MAX_DIR_ENTRIES_PER_PAGE 99
#define MAX_BLOCK_ENTRIES_PER_PAGE 100
/* Minimum number of entries before an underflow */
#define MIN_DIR_ENTRIES_PER_PAGE 30
/* WARNING: MIN_DIR_ENTRIES_PER_PAGE must be smaller than
*  MAX_DIR_ENTRIES_PER_PAGE/2 */

/* Max length of link path pointed by symbolic link */
#define MAX_LINK_PATH 4096

/* Hard link limit */
#define MAX_HARD_LINK 65000

/* Number of pointers in a pointer page */
#define POINTERS_PER_PAGE 1024

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
#define ATIME 4
#define MTIME 2
#define CTIME 1

#define FUSE_SOCK_PATH "/dev/shm/fuse_communication_reporter"

/* Defines for local pin property */
#define NUM_PIN_TYPES 3
#define P_UNPIN 0
#define P_PIN 1
#define P_HIGH_PRI_PIN 2
/* Marcos to check pin property */
#define P_IS_VALID_PIN(p) (p < NUM_PIN_TYPES) /* To valid pin property */
#define P_IS_PIN(p) ((p == P_PIN) || (p == P_HIGH_PRI_PIN))
#define P_IS_UNPIN(p) (p == P_UNPIN)

#ifdef _ANDROID_ENV_
#define  IS_ANDROID_EXTERNAL(type) (((type) == (ANDROID_EXTERNAL)) || \
		((type) == (ANDROID_MULTIEXTERNAL)))
#endif

/* Structures for directories */
/* Defining directory entry in meta files*/
typedef struct {
	ino_t d_ino;
	char d_name[MAX_FILENAME_LEN+1];
	char d_type;
} DIR_ENTRY;

/* Defining the structure of directory object meta */
typedef struct {
	int64_t total_children;  /*Total children not including "." and "..*/
	int64_t root_entry_page;
	int64_t next_xattr_page;
	int64_t entry_page_gc_list;
	int64_t tree_walk_list_head;
	uint64_t generation;
	uint8_t source_arch;
	uint64_t metaver;
	ino_t root_inode;
	int64_t finished_seq;
	char local_pin;
} DIR_META_TYPE;

/* Defining the structure for a page of directory entries */
typedef struct {
	int32_t num_entries;
	DIR_ENTRY dir_entries[MAX_DIR_ENTRIES_PER_PAGE];
	int64_t this_page_pos; /*File pos of the current node*/
	/* File pos of child pages for this node, b-tree style */
	int64_t child_page_pos[MAX_DIR_ENTRIES_PER_PAGE+1];
	/*File pos of parent. If this is the root, the value is 0 */
	int64_t parent_page_pos;
	/*File pos of the next gc entry if on gc list*/
	int64_t gc_list_next;
	int64_t tree_walk_next;
	int64_t tree_walk_prev;
} DIR_ENTRY_PAGE;

/* Structures for regular files */
/* Defining one block status entry in meta files */
typedef struct {
	uint8_t status;
	uint8_t uploaded;
#if (DEDUP_ENABLE)
	uint8_t obj_id[OBJID_LENGTH];
#endif
	uint32_t paged_out_count;
	int64_t seqnum;
} BLOCK_ENTRY;

/* Defining the structure of one page of block status page */
typedef struct {
	int32_t num_entries;
	BLOCK_ENTRY block_entries[MAX_BLOCK_ENTRIES_PER_PAGE];
} BLOCK_ENTRY_PAGE;

/* Defining the structure of pointer page (pointers to other pages) */
typedef struct {
	int64_t ptr[POINTERS_PER_PAGE];
} PTR_ENTRY_PAGE;

/* Defining the structure of file meta */
typedef struct {
	int64_t next_xattr_page;
	int64_t direct;
	int64_t single_indirect;
	int64_t double_indirect;
	int64_t triple_indirect;
	int64_t quadruple_indirect;
	uint64_t generation;
        uint8_t source_arch;
	uint64_t metaver;
	ino_t root_inode;
	int64_t finished_seq;
	char local_pin;
} FILE_META_TYPE;

/* The structure for keeping statistics for a file */
typedef struct {
	int64_t num_blocks;
	int64_t num_cached_blocks;
	int64_t cached_size;
	int64_t dirty_data_size;
} FILE_STATS_TYPE;

/* Defining the structure of symbolic link meta */
typedef struct {
	int64_t next_xattr_page;
	uint32_t link_len;
	uint64_t generation;
	char link_path[MAX_LINK_PATH]; /* NOT null-terminated string */
        uint8_t source_arch;
	uint64_t metaver;
	ino_t root_inode;
	int64_t finished_seq;
	char local_pin;
} SYMLINK_META_TYPE;

typedef struct {
	int64_t size_last_upload; /* Record data + meta */
	int64_t meta_last_upload; /* Record meta only */
	int64_t upload_seq;
} CLOUD_RELATED_DATA;

/*END META definition*/

/* Defining the system meta resources */
typedef struct {
	int64_t system_size; /* data + meta + sb */
	int64_t system_meta_size; /* meta */
	int64_t super_block_size; /* sb */
	int64_t cache_size; /* data(local) + meta + sb */
	int64_t cache_blocks;
	int64_t pinned_size; /* data(pin) + meta + sb */
	int64_t unpin_dirty_data_size; /* dirty data w/ unpin property */
	int64_t backend_size; /* data(sync) + meta(sync) */
	int64_t backend_meta_size;
	int64_t backend_inodes;
	int64_t dirty_cache_size; /* data + meta */
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

	/* Semaphore for controlling cache management */
	sem_t something_to_replace;

	/* system state controllers */
	BOOL system_going_down;
	BOOL backend_is_online;
	BOOL sync_manual_switch;
	BOOL sync_paused;
	BOOL xfer_upload_in_progress;
	BOOL writing_sys_data;

	/* Lots of functions will invoke download directly */
	sem_t xfer_download_in_progress_sem;
	/* Xfer window must be shifted in an interval */
	time_t last_xfer_shift_time;

	BOOL system_restoring;
	struct timespec backend_status_last_time;
} SYSTEM_DATA_HEAD;

SYSTEM_DATA_HEAD *hcfs_system;

int32_t global_argc;
char **global_argv;
struct fuse_args global_fuse_args;

/* Functions for initializing HCFS */
void *mount_multi_thread(void *ptr);
void *mount_single_thread(void *ptr);

int32_t hook_fuse(int32_t argc, char **argv);

ino_t data_data_root;

void set_timestamp_now(struct stat *thisstat, char mode);
#endif  /* GW20_HCFS_FUSEOP_H_ */
