/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfs_cachebuild.h
* Abstract: The c header file for building structure for cache usage.
*
* Revision History
* 2015/2/11 Jiahong created this file from part of hcfs_cache.h.
* 2015/2/11 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/
#ifndef GW20_HCFS_HCFS_CACHEBUILD_H_
#define GW20_HCFS_HCFS_CACHEBUILD_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>

#define CACHE_USAGE_NUM_ENTRIES 8

typedef struct usage_node_template {
	ino_t this_inode;
	int64_t clean_cache_size;
	int64_t dirty_cache_size;
	time_t last_access_time;
	time_t last_mod_time;
	struct usage_node_template *next_node;
} CACHE_USAGE_NODE;

CACHE_USAGE_NODE *inode_cache_usage_hash[CACHE_USAGE_NUM_ENTRIES];
int32_t nonempty_cache_hash_entries;

int32_t cache_usage_hash_init(void);

/*Pops the entry from the linked list if found, else NULL is returned*/
CACHE_USAGE_NODE *return_cache_usage_node(ino_t this_inode);
void insert_cache_usage_node(ino_t this_inode, CACHE_USAGE_NODE *this_node);

/*For compare_cache_usage, returns a negative number (< 0) if the first node
needs to be placed in front of the second node, returns zero if does not matter,
returns a positive number (> 0) if the second node needs to be placed in front
of the first node*/
int32_t compare_cache_usage(CACHE_USAGE_NODE *first_node,
					CACHE_USAGE_NODE *second_node);
int32_t build_cache_usage(void);

#endif  /* GW20_HCFS_HCFS_CACHEBUILD_H_ */
