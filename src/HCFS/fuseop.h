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
*
**************************************************************************/

#ifndef GW20_HCFS_FUSEOP_H_
#define GW20_HCFS_FUSEOP_H_


#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>

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

/* Structures for directories */
/* Defining directory entry in meta files*/
typedef struct {
	ino_t d_ino;
	char d_name[256];
	char d_type;
} DIR_ENTRY;

/* Defining the structure of directory object meta */
typedef struct {
	long long total_children;  /*Total children not including "." and "..*/
	long long root_entry_page;
	long long next_xattr_page;
	long long entry_page_gc_list;
	long long tree_walk_list_head;
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
} BLOCK_ENTRY;

/* Defining the structure of file meta */
typedef struct {
	long long next_block_page;
	long long next_xattr_page;
} FILE_META_TYPE;

/* Defining the structure of one page of block status page */
typedef struct {
	int num_entries;
	BLOCK_ENTRY block_entries[MAX_BLOCK_ENTRIES_PER_PAGE];
	long long next_page;
} BLOCK_ENTRY_PAGE;

/*END META definition*/

/* Defining the system meta resources */
typedef struct {
	long long system_size;
	long long cache_size;
	long long cache_blocks;
} SYSTEM_DATA_TYPE;

typedef struct {
	FILE *system_val_fptr;
	SYSTEM_DATA_TYPE systemdata;
	sem_t access_sem;
	sem_t num_cache_sleep_sem;
	sem_t check_cache_sem;
	sem_t check_next_sem;
} SYSTEM_DATA_HEAD;

SYSTEM_DATA_HEAD *hcfs_system;

/* Other system-related resources */
FILE *logfptr;  /* File pointer to logs */
pthread_t reporter_thread;  /* Used by utility prototype */

/* Functions for initializing HCFS */
int hook_fuse(int argc, char **argv);

#endif  /* GW20_HCFS_FUSEOP_H_ */
