/*************************************************************************
*
* Copyright © 2014-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: meta.h
* Abstract: The header file for hcfs metafile structure
*
* Revision History
* 2016/7/11 Jethro moved meta structures from fuseop.h to meta.h
* 2016/7/12 Jethro add version v1, moved origin structure and HCFS_STAT into meta.h
*
**************************************************************************/

#ifndef SRC_HCFS_META_H_
#define SRC_HCFS_META_H_

#include <assert.h>
#include <sys/stat.h>
#include "macro.h"

/******************************************************************************
 * BEGIN META definition
 *****************************************************************************/

/* Defining parameters for B-tree operations (dir entries).
 *
 * MAX_BLOCK_ENTRIES_PER_PAGE:
 * 	Max number of children per node is 100, min is 50, so at least 49
 * 	elements in each node (except the root)
 * MIN_DIR_ENTRIES_PER_PAGE:
 * 	Minimum number of entries before an underflow.
 * 	WARNING: MIN_DIR_ENTRIES_PER_PAGE must be smaller than
 * 	MAX_DIR_ENTRIES_PER_PAGE/2
 * POINTERS_PER_PAGE:
 * 	Number of pointers in a pointer page
 * MAX_LINK_PATH:
 * 	Max length of link path pointed by symbolic link
 */

/* Current version value */
#define MAX_BLOCK_ENTRIES_PER_PAGE 100
#define   MAX_DIR_ENTRIES_PER_PAGE 99
#define   MIN_DIR_ENTRIES_PER_PAGE 30
#define          POINTERS_PER_PAGE 1024
#define              MAX_LINK_PATH 4096

/* All versions value */
#define   MAX_DIR_ENTRIES_PER_PAGE_v1 99
#define   MIN_DIR_ENTRIES_PER_PAGE_v1 30
#define          POINTERS_PER_PAGE_v1 1024
#define              MAX_LINK_PATH_v1 4096

static const char META_MAGIC[] = "hcfs";

typedef struct { /* 128 bytes */
	uint8_t magic[4];
	uint32_t metaver;
	uint64_t dev;
	uint64_t ino;
	uint32_t mode;
	uint32_t __pad1;
	uint64_t nlink; /* unsigned int in android */
	uint32_t uid;
	uint32_t gid;
	uint64_t rdev;
	int64_t size;
	int64_t blksize; /* int in android */
	int64_t blocks;
	int64_t atime; /* use aarch64 time structure */
	uint64_t atime_nsec;
	int64_t mtime;
	uint64_t mtime_nsec;
	int64_t ctime;
	uint64_t ctime_nsec;
} HCFS_STAT, HCFS_STAT_v1;

#define COMMON_STAT_MEMBER \
	X(dev);     X(ino);        X(mode);  X(nlink);      \
	X(uid);     X(gid);        X(rdev);  X(size);       \
	X(blksize); X(blocks);

/******************************************************************************
 * XATTR definition
 *****************************************************************************/
/* Current version value */
#define MAX_KEY_SIZE           256 /* Max key length */
#define MAX_VALUE_BLOCK_SIZE   256 /* Max value size per block (256) */
#define MAX_KEY_ENTRY_PER_LIST 4   /* Max key entry of the sorted array (4) */
#define MAX_KEY_HASH_ENTRY     8   /* Max hash table entries (8) */


/* Old version value */
#define MAX_KEY_SIZE_v1           256 /* Max key length */
#define MAX_VALUE_BLOCK_SIZE_v1   256 /* Max value size per block (256) */
#define MAX_KEY_ENTRY_PER_LIST_v1 4   /* Max key entry of the sorted array (4) */
#define MAX_KEY_HASH_ENTRY_v1     8   /* Max hash table entries (8) */

/* Struct of VALUE_BLOCK. Value of an extened attr is stored using linked
   VALUE_BLOCK, and it will be reclaimed if xattr is removed. */
typedef struct {
	char content[MAX_VALUE_BLOCK_SIZE]; /* Content is NOT null-terminated */
	int64_t next_block_pos;
} VALUE_BLOCK, VALUE_BLOCK_v1;

/* A key entry includes key size, value size, the key string, and a file
   offset pointing to first value block. */
typedef struct {
	uint32_t key_size;
	uint32_t value_size;
	char key[MAX_KEY_SIZE]; /* Key is null-terminated string  */
	int64_t first_value_block_pos;
} KEY_ENTRY, KEY_ENTRY_v1;

/* KEY_LIST includes an array sorted by key, and number of xattr.
   If the KEY_LIST is the first one, prev_list_pos is set to 0. If it is the
   last one, then next_list_pos is set to 0. */
typedef struct {
	uint32_t num_xattr;
	KEY_ENTRY key_list[MAX_KEY_ENTRY_PER_LIST];
	int64_t next_list_pos;
} KEY_LIST_PAGE, KEY_LIST_PAGE_v1;

/* NAMESPACE_PAGE includes a hash table which is used to hash the input key.
   Each hash entry points to a KEY_LIST. */
typedef struct {
	uint32_t num_xattr;
	int64_t key_hash_table[MAX_KEY_HASH_ENTRY];
} NAMESPACE_PAGE, NAMESPACE_PAGE_v1;

/* XATTR_PAGE is pointed by next_xattr_page in meta file. Namespace is one of
   user, system, security, and trusted. */
typedef struct {
	int64_t reclaimed_key_list_page;
	int64_t reclaimed_value_block;
	NAMESPACE_PAGE namespace_page[4];
} XATTR_PAGE, XATTR_PAGE_v1;


typedef struct {
	int64_t size_last_upload; /* Record data + meta */
	int64_t meta_last_upload; /* Record meta only */
	int64_t upload_seq;
} CLOUD_RELATED_DATA, CLOUD_RELATED_DATA_v1;

/******************************************************************************
 * Structures for directories
 *****************************************************************************/
/* Defining directory entry in meta files*/
typedef struct {
	uint64_t d_ino;
	char d_name[MAX_FILENAME_LEN + 1];
	char d_type;
	uint8_t padding[6];
} DIR_ENTRY, DIR_ENTRY_v1;

/* Defining the structure of directory object meta */
typedef struct {
	int64_t total_children; /*Total children not including "." and "..*/
	int64_t root_entry_page;
	int64_t next_xattr_page;
	int64_t entry_page_gc_list;
	int64_t tree_walk_list_head;
	uint64_t generation;
	uint8_t source_arch;
	uint64_t root_inode;
	int64_t finished_seq;
	uint8_t local_pin;
	uint8_t padding[7];
} DIR_META_TYPE, DIR_META_TYPE_v1;

/* Defining the structure for a page of directory entries */
typedef struct {
	int32_t num_entries;
	DIR_ENTRY dir_entries[MAX_DIR_ENTRIES_PER_PAGE];
	int64_t this_page_pos; /*File pos of the current node*/
	/* File pos of child pages for this node, b-tree style */
	int64_t child_page_pos[MAX_DIR_ENTRIES_PER_PAGE + 1];
	/*File pos of parent. If this is the root, the value is 0 */
	int64_t parent_page_pos;
	/*File pos of the next gc entry if on gc list*/
	int64_t gc_list_next;
	int64_t tree_walk_next;
	int64_t tree_walk_prev;
} DIR_ENTRY_PAGE, DIR_ENTRY_PAGE_v1;

typedef struct {
	HCFS_STAT st;
	DIR_META_TYPE dmt;
	CLOUD_RELATED_DATA crd;
	uint8_t padding[64];
} DIR_META_HEADER, DIR_META_HEADER_v1;

/******************************************************************************
 * Structures for regular files
 *****************************************************************************/
/* Defining one block status entry in meta files */
typedef struct {
	uint8_t status;
	uint8_t uploaded;
#if (DEDUP_ENABLE)
	uint8_t obj_id[OBJID_LENGTH];
#endif
	uint32_t paged_out_count;
	int64_t seqnum;
} BLOCK_ENTRY, BLOCK_ENTRY_v1;

/* Defining the structure of one page of block status page */
typedef struct {
	int32_t num_entries;
	BLOCK_ENTRY block_entries[MAX_BLOCK_ENTRIES_PER_PAGE];
} BLOCK_ENTRY_PAGE, BLOCK_ENTRY_PAGE_v1;

/* Defining the structure of pointer page (pointers to other pages) */
typedef struct {
	int64_t ptr[POINTERS_PER_PAGE];
} PTR_ENTRY_PAGE, PTR_ENTRY_PAGE_v1;

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
	uint64_t root_inode;
	int64_t finished_seq;
	uint8_t local_pin;
	uint8_t padding[7]; // gcc auto pad to 8 byte even without this
} FILE_META_TYPE, FILE_META_TYPE_v1;

/* The structure for keeping statistics for a file */
typedef struct {
	int64_t num_blocks;
	int64_t num_cached_blocks;
	int64_t cached_size;
	int64_t dirty_data_size;
} FILE_STATS_TYPE, FILE_STATS_TYPE_v1;

typedef struct {
	HCFS_STAT st;
	FILE_META_TYPE fmt;
	FILE_STATS_TYPE fst;
	CLOUD_RELATED_DATA crd;
	uint8_t padding[64];
} FILE_META_HEADER, FILE_META_HEADER_v1;

/******************************************************************************
 * Defining the structure of symbolic link meta
 *****************************************************************************/
typedef struct {
	int64_t next_xattr_page;
	uint32_t link_len;
	uint64_t generation;
	char link_path[MAX_LINK_PATH]; /* NOT null-terminated string */
	uint8_t source_arch;
	uint64_t root_inode;
	int64_t finished_seq;
	uint8_t local_pin;
	uint8_t padding[7];
} SYMLINK_META_TYPE, SYMLINK_META_TYPE_v1;

typedef struct {
	HCFS_STAT st;
	SYMLINK_META_TYPE smt;
	CLOUD_RELATED_DATA crd;
} SYMLINK_META_HEADER, SYMLINK_META_HEADER_v1;

/*END META definition*/



/******************************************************************************
 * BEGIN OLD META definition
 *****************************************************************************/

/* Defining the structure of directory object meta */

/*END OLD META definition*/

/*******************************************************************************
 * Guardian of structure version
 *
 * Make sure changes of structure are proper handled properly
 *
 * Since we have padding in DIR_META_HEADER and FILE_META_HEADER, the size of
 * structures should not change frequently. If (but not limit to) asserts
 * failed here, meta structure are changed and you should:
 * 	- Adjust padding size to keep structure size unchanged. This can reduce
 * 	the cost on updating metafile.
 * 	- Split out definition of old version and keep them in bottom of meta.h
 * 	- Increase CURRENT_META_VER
 * 	- Handle meta version conversion in metaops.c
 ******************************************************************************/


void init_hcfs_stat(HCFS_STAT *this_stat);
void convert_hcfsstat_to_sysstat(struct stat *ret_stat, HCFS_STAT *tmp_stat);
void set_timestamp_now(HCFS_STAT *thisstat, char mode);

#endif /* SRC_HCFS_META_H_ */
