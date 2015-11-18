/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: meta_mem_cache.h
* Abstract: The c header file for meta cache operations in HCFS.
*
* Revision History
* 2015/2/9 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/

#ifndef GW20_HCFS_META_MEM_CACHE_H_
#define GW20_HCFS_META_MEM_CACHE_H_

/* TODO: Perhaps don't want to cache xattr for directory objects */

/* A global meta cache (Done) and a block data cache (TODO) in memory.
All reads / writes go through the caches, and a parameter (TODO) controls
when to write dirty cache entries back to files (could be write through or
after several seconds). */

/* A hard limit defines the upper bound on the number of entries
	(or mem used?) */
/* Dynamically allocate memory and release memory when not being used
	for a long time (controlled by a parameter) */

/* Will keep cache entry even after file is closed, until expired or need
	to be replaced */

/* Cannot open more file if all meta cache entry is occupied by opened files */

/* Data structure

Each meta cache entry keeps
1. Struct stat
2. Struct DIR_META_TYPE or FILE_META_TYPE
3. Up to two dir entry pages cached
4. Up to two block entry pages cached  (deleted)
5. Up to two xattr pages cached (pending)
6. Number of opened handles to the inode
7. Semaphore to the entry
8. Last access time
9. Dirty or clean status for items 1 to 5

Lookup of cache entry:

inode => hashtable lookup (hash table with doubly linked list) =>
		return pointer to the entry
Each lookup entry contains the doubly linked list structure, plus number
of opened handles to the inode and the inode number. Finally, the pointer
to the actual cache entry is stored.

Each entry in the hashtable (the header of each linked list) contains a
semaphore to this list. If two accesses collide at the same time, one
has to wait for the completion of the other. This is to ensure the
atomic completion of adding and deleting cache entries.
If deleting cache entry, will need to acquire both the header lock and
the entry lock before proceeding. If cache entry lock cannot be acquired
immediately, should release header lock and sleep for a short time, or skip
to other entries.
*/

#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include "fuseop.h"

typedef struct {
	struct stat this_stat;
	ino_t inode_num;
	char stat_dirty;
	DIR_META_TYPE *dir_meta;  /* Only used if inode is a dir */
	FILE_META_TYPE *file_meta;  /* Only used if inode is a reg file */
	SYMLINK_META_TYPE *symlink_meta; /* Only used if inode is a symlink */
	char meta_dirty;

	/*index 0 means newer entry, index 1 means older.
	Always first flush index 1, copy index 0 to 1,
	then put new page to index 0 */

	/*Zero if not pointed to any page*/
	DIR_ENTRY_PAGE * dir_entry_cache[2];
	char dir_entry_cache_dirty[2];

	sem_t access_sem;
	char something_dirty;
	char meta_opened;
	FILE *fptr;

	/*TODO: Need to think whether system clock change could affect the
	involved operations*/
	struct timeval last_access_time;
} META_CACHE_ENTRY_STRUCT;

struct meta_cache_lookup_struct {
	META_CACHE_ENTRY_STRUCT body;
	ino_t inode_num;
	struct meta_cache_lookup_struct *next;
	struct meta_cache_lookup_struct *prev;
};

typedef struct meta_cache_lookup_struct META_CACHE_LOOKUP_ENTRY_STRUCT;

typedef struct {
	META_CACHE_LOOKUP_ENTRY_STRUCT *meta_cache_entries;
	sem_t header_sem;
	int num_entries;
	META_CACHE_LOOKUP_ENTRY_STRUCT *last_entry;
} META_CACHE_HEADER_STRUCT;

int init_meta_cache_headers(void);
int release_meta_cache_headers(void);
int flush_single_entry(META_CACHE_ENTRY_STRUCT *body_ptr);
int meta_cache_flush_dir_cache(META_CACHE_ENTRY_STRUCT *body_ptr,
							int entry_index);
int flush_clean_all_meta_cache(void);
int free_single_meta_cache_entry(META_CACHE_LOOKUP_ENTRY_STRUCT *entry_ptr);

int meta_cache_update_file_data(ino_t this_inode, const struct stat *inode_stat,
	const FILE_META_TYPE *file_meta_ptr, const BLOCK_ENTRY_PAGE *block_page,
	const long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr);

int meta_cache_lookup_file_data(ino_t this_inode, struct stat *inode_stat,
	FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
	long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr);

int meta_cache_update_dir_data(ino_t this_inode, const struct stat *inode_stat,
	const DIR_META_TYPE *dir_meta_ptr, const DIR_ENTRY_PAGE *dir_page,
	META_CACHE_ENTRY_STRUCT *bptr);

int meta_cache_lookup_dir_data(ino_t this_inode, struct stat *inode_stat,
	DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page,
	META_CACHE_ENTRY_STRUCT *body_ptr);

int meta_cache_seek_dir_entry(ino_t this_inode, DIR_ENTRY_PAGE *result_page,
	int *result_index, const char *childname,
		META_CACHE_ENTRY_STRUCT *body_ptr);

int meta_cache_remove(ino_t this_inode);
int meta_cache_push_dir_page(META_CACHE_ENTRY_STRUCT *body_ptr,
				const DIR_ENTRY_PAGE *temppage);


int meta_cache_update_symlink_data(ino_t this_inode,
				   const struct stat *inode_stat,
				   const SYMLINK_META_TYPE *symlink_meta_ptr,
				   META_CACHE_ENTRY_STRUCT *bptr);

int meta_cache_lookup_symlink_data(ino_t this_inode, struct stat *inode_stat,
	SYMLINK_META_TYPE *symlink_meta_ptr, META_CACHE_ENTRY_STRUCT *body_ptr);

META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode);
int meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr);
int meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr);
int meta_cache_close_file(META_CACHE_ENTRY_STRUCT *body_ptr);
int meta_cache_drop_pages(META_CACHE_ENTRY_STRUCT *body_ptr);

int expire_meta_mem_cache_entry(void);

#endif  /* GW20_HCFS_META_MEM_CACHE_H_ */
