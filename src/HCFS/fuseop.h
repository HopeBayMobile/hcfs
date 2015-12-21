/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
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

/* Structures for directories */
/* Defining directory entry in meta files*/
typedef struct {
	ino_t d_ino;
	char d_name[MAX_FILENAME_LEN+1];
	char d_type;
} DIR_ENTRY;

/* Defining the structure of directory object meta */
typedef struct {
	long long total_children;  /*Total children not including "." and "..*/
	long long root_entry_page;
	long long next_xattr_page;
	long long entry_page_gc_list;
	long long tree_walk_list_head;
	unsigned long generation;
	unsigned char source_arch;
	unsigned long long metaver;
	ino_t root_inode;
	long long upload_seq;
	char local_pin;
} DIR_META_TYPE;

/* Defining the structure for a page of directory entries */
typedef struct {
	int num_entries;
	DIR_ENTRY dir_entries[MAX_DIR_ENTRIES_PER_PAGE];
	long long this_page_pos; /*File pos of the current node*/
	/* File pos of child pages for this node, b-tree style */
	long long child_page_pos[MAX_DIR_ENTRIES_PER_PAGE+1];
	/*File pos of parent. If this is the root, the value is 0 */
	long long parent_page_pos;
	/*File pos of the next gc entry if on gc list*/
	long long gc_list_next;
	long long tree_walk_next;
	long long tree_walk_prev;
} DIR_ENTRY_PAGE;

/* Structures for regular files */
/* Defining one block status entry in meta files */
typedef struct {
	unsigned char status;
	unsigned char uploaded;
#if (DEDUP_ENABLE)
	unsigned char obj_id[OBJID_LENGTH];
#endif
} BLOCK_ENTRY;

/* Defining the structure of one page of block status page */
typedef struct {
	int num_entries;
	BLOCK_ENTRY block_entries[MAX_BLOCK_ENTRIES_PER_PAGE];
} BLOCK_ENTRY_PAGE;

/* Defining the structure of pointer page (pointers to other pages) */
typedef struct {
	long long ptr[POINTERS_PER_PAGE];
} PTR_ENTRY_PAGE;

/* Defining the structure of file meta */
typedef struct {
	long long next_xattr_page;
	long long direct;
	long long single_indirect;
	long long double_indirect;
	long long triple_indirect;
	long long quadruple_indirect;
	unsigned long generation;
        unsigned char source_arch;
	unsigned long long metaver;
	ino_t root_inode;
	long long upload_seq;
	long long size_last_upload;
	char local_pin;
} FILE_META_TYPE;

/* The structure for keeping statistics for a file */
typedef struct {
	long long num_blocks;
	long long num_cached_blocks;
	long long cached_size;
} FILE_STATS_TYPE;

/* Defining the structure of symbolic link meta */
typedef struct {
	long long next_xattr_page;
	unsigned link_len;
	unsigned long generation;
	char link_path[MAX_LINK_PATH]; /* NOT null-terminated string */
        unsigned char source_arch;
	unsigned long long metaver;
	ino_t root_inode;
	long long upload_seq;
	char local_pin;
} SYMLINK_META_TYPE;

/*END META definition*/

/* Defining the system meta resources */
typedef struct {
	long long system_size;
	long long cache_size;
	long long cache_blocks;
	long long pinned_size;
	long long backend_size;
	long long backend_inodes;
	long long dirty_cache_size;
	long long xfer_size_download;
	long long xfer_size_upload;
	int cache_replace_status;
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
	/* system state controllers */
	BOOL system_going_down;
	BOOL backend_is_online;
	BOOL sync_manual_switch;
	BOOL sync_paused;
	struct timespec backend_status_last_time;
} SYSTEM_DATA_HEAD;

SYSTEM_DATA_HEAD *hcfs_system;

int global_argc;
char **global_argv;
struct fuse_args global_fuse_args;

/* Functions for initializing HCFS */
void *mount_multi_thread(void *ptr);
void *mount_single_thread(void *ptr);

int hook_fuse(int argc, char **argv);

void set_timestamp_now(struct stat *thisstat, char mode);
#endif  /* GW20_HCFS_FUSEOP_H_ */
