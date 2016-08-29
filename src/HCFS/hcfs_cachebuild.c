/*************************************************************************
*
* Copyright © 2014-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hfuse_cachebuild.c
* Abstract: The c source code file for building structure for cache usage.
*
* Revision History
* 2015/2/11 Jiahong added header for this file, and revising coding style.
* 2016/5/23 Jiahong adding more cache mgmt control.
*
**************************************************************************/

#include "hcfs_cachebuild.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/mman.h>
#include <inttypes.h>

#include "params.h"
#include "fuseop.h"
#include "super_block.h"
#include "logger.h"
#include "global.h"
#include "utils.h"

/************************************************************************
*
* Function name: cache_usage_hash_init
*        Inputs: None
*       Summary: Initialize cache usage structure.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t cache_usage_hash_init(void)
{
	int32_t count;
	CACHE_USAGE_NODE *node_ptr, *temp_ptr;

	nonempty_cache_hash_entries = 0;
	for (count = 0; count < CACHE_USAGE_NUM_ENTRIES; count++) {
		node_ptr = inode_cache_usage_hash[count];
		while (node_ptr != NULL) {
			temp_ptr = node_ptr;
			node_ptr = node_ptr->next_node;
			free(temp_ptr);
		}
		inode_cache_usage_hash[count] = NULL;
	}
	return 0;
}

/************************************************************************
*
* Function name: return_cache_usage_node
*        Inputs: ino_t this_inode
*       Summary: Find if there is a cache usage node for "this_inode" in
*                the cache usage hash-table / linked list structure. If
*                there found, remove the node from the structure and return
*                the node to the caller.
*  Return value: Pointer to the usage node if successful.
*                Otherwise returns NULL.
*
*************************************************************************/
CACHE_USAGE_NODE *return_cache_usage_node(ino_t this_inode)
{
	int32_t hash_value;
	CACHE_USAGE_NODE *node_ptr, *next_ptr;

	hash_value = this_inode % CACHE_USAGE_NUM_ENTRIES;

	node_ptr = inode_cache_usage_hash[hash_value];

	if (node_ptr == NULL)
		return NULL;

	if (node_ptr->this_inode == this_inode) {
		inode_cache_usage_hash[hash_value] = node_ptr->next_node;
		if (node_ptr->next_node == NULL)
			nonempty_cache_hash_entries--;
		node_ptr->next_node = NULL;
		return node_ptr;
	}

	next_ptr = node_ptr->next_node;

	while (next_ptr != NULL) {
		if (next_ptr->this_inode == this_inode) {
			node_ptr->next_node = next_ptr->next_node;
			next_ptr->next_node = NULL;
			return next_ptr;
		}
		node_ptr = next_ptr;
		next_ptr = next_ptr->next_node;
	}

	return NULL;
}

/************************************************************************
*
* Function name: insert_cache_usage_node
*        Inputs: ino_t this_inode, CACHE_USAGE_NODE *this_node
*       Summary: Insert the cache usage node ("this_node") of inode "this_inode"
*                to the cache usage structure. Position in the linked list
*                of a hash table entry is determined by the function
*                compare_cache_usage.
*  Return value: None
*
*************************************************************************/
void insert_cache_usage_node(ino_t this_inode, CACHE_USAGE_NODE *this_node)
{
	int32_t hash_value;
	CACHE_USAGE_NODE *node_ptr, *next_ptr;

	hash_value = this_inode % CACHE_USAGE_NUM_ENTRIES;

	node_ptr = inode_cache_usage_hash[hash_value];

	if (node_ptr == NULL) { /*First one, so just insert*/
		inode_cache_usage_hash[hash_value] = this_node;
		this_node->next_node = NULL;
		nonempty_cache_hash_entries++;
		return;
	}

	if (compare_cache_usage(this_node, node_ptr) <= 0) {
		/*The new node is the first one*/
		this_node->next_node = node_ptr;
		inode_cache_usage_hash[hash_value] = this_node;
		return;
	}

	next_ptr = node_ptr->next_node;

	while (next_ptr != NULL) {
		/*If insert new node at a latter place */
		if (compare_cache_usage(this_node, next_ptr) > 0) {
			node_ptr = next_ptr;
			next_ptr = next_ptr->next_node;
		} else {
			break;
		}
	}

	node_ptr->next_node = this_node;
	this_node->next_node = next_ptr;
}

/************************************************************************
*
* Function name: compare_cache_usage
*        Inputs: CACHE_USAGE_NODE *first_node, CACHE_USAGE_NODE *second_node
*       Summary: Compare the content of two usage nodes, and return 1, -1, or 0.
*  Return value: 1 if the first node should be placed after the second
*                one in the linked list.
*                -1 if the first node should be placed before the second
*                one in the linked list.
*                0 if does not matter.
*
*************************************************************************/
int32_t compare_cache_usage(CACHE_USAGE_NODE *first_node,
					CACHE_USAGE_NODE *second_node)
{
	time_t first_node_time, second_node_time;

	/*If clean cache size is zero, put it to the end of the linked lists*/
	if (first_node->clean_cache_size == 0)
		return 1;

	if (second_node->clean_cache_size == 0)
		return -1;

	if (first_node->last_access_time > first_node->last_mod_time)
		first_node_time = first_node->last_access_time;
	else
		first_node_time = first_node->last_mod_time;

	if (second_node->last_access_time > second_node->last_mod_time)
		second_node_time = second_node->last_access_time;
	else
		second_node_time = second_node->last_mod_time;

	/*Use access/mod time to rank*/

	if (first_node_time > (second_node_time + 60))
		return 1;

	if (second_node_time > (first_node_time + 60))
		return -1;

	/*Use clean cache size to rank*/

	if (first_node->clean_cache_size > (second_node->clean_cache_size +
								MAX_BLOCK_SIZE))
		return -1;

	if (second_node->clean_cache_size > (first_node->clean_cache_size +
								MAX_BLOCK_SIZE))
		return 1;

	/*Use dirty cache size to rank*/

	if (first_node->dirty_cache_size > second_node->dirty_cache_size)
		return 1;

	if (first_node->dirty_cache_size < second_node->dirty_cache_size)
		return -1;

	return 0;
}

/************************************************************************
*
* Function name: build_cache_usage
*        Inputs: None
*       Summary: Build the cache usage structure based on the current local
*                block cache status.
*  Return value: 0 if successful, otherwise negation of error code.
*
* Note: Will continue to scan for cache usage even if encountered some error
*************************************************************************/
int32_t build_cache_usage(void)
{
	char blockpath[400];
	char thisblockpath[400];
	DIR *dirptr;
	int32_t count;
	struct dirent *de;
	int32_t ret, errcode;
	int64_t blockno;
	int64_t block_size_blk;
	ino_t this_inode;
	struct stat tempstat; /* block ops */
	CACHE_USAGE_NODE *tempnode;
	size_t tmp_size;
	char is_dirty;

	write_log(5, "Building cache usage hash table\n");
	ret = cache_usage_hash_init();
	if (ret < 0)
		return ret;

	write_log(10, "Initialized cache usage hash table\n");

	/* Marking something_to_replace as false */
	do {
		ret = sem_trywait(&(hcfs_system->something_to_replace));
		if (ret < 0) {
			errcode = (int32_t) errno;
			if (errcode != EAGAIN)
				return -errcode;
		} else {
			errcode = 0;
		}
	} while (errcode == 0);

	for (count = 0; count < NUMSUBDIR; count++) {
		write_log(10, "Now processing subfolder %d\n", count);
		sprintf(blockpath, "%s/sub_%d", BLOCKPATH, count);

		if (access(blockpath, F_OK) < 0)
			continue;

		dirptr = opendir(blockpath);

		if (dirptr == NULL) {
			errcode = errno;
			write_log(0, "Error in cache replacement. Skipping\n");
			write_log(2, "Code %d, %s\n", errcode,
				strerror(errcode));
			continue;
		}

		errno = 0; de = readdir(dirptr);
		if (de == NULL && errno) {
			errcode = errno;
			write_log(0, "Error in cache replacement. Skipping\n");
			write_log(2, "Code %d, %s\n", errcode,
				strerror(errcode));
			closedir(dirptr);
			continue;
		}
	//	write_log(10, "count is now %d\n", count);

		while (de != NULL) {
	//		write_log(10, "count is now %d\n", count);
			write_log(10, "Scanning file name %s\n",
			          de->d_name);
			if (hcfs_system->system_going_down == TRUE)
				break;
			errcode = 0;
			ret = sscanf(de->d_name, "block%" PRIu64 "_%" PRId64,
			             (uint64_t *)&this_inode, &blockno);
			if (ret != 2) {
				write_log(10, "Scan file does not match\n");
				errno = 0; de = readdir(dirptr);
				if (de == NULL && errno) {
					errcode = errno;
					break;
				}
				continue;
			}
			write_log(10, "Count is now %d, %lu, %lu\n", count,
			          (uint64_t)&count, (uint64_t)&blockno);
			write_log(10, "Block file for %" PRIu64 " %lld\n",
			          (uint64_t)this_inode, blockno);
			ret = fetch_block_path(thisblockpath, this_inode,
						blockno);
			if (ret < 0) {
				errcode = ret;
				break;
			}
			write_log(10, "Debug %s, %d\n", thisblockpath, count);
			ret = stat(thisblockpath, &tempstat);

			if (ret != 0) {
				errno = 0; de = readdir(dirptr);
				if (de == NULL && errno) {
					errcode = errno;
					break;
				}
				continue;
			}

			write_log(10, "Fetching cache usage node\n");
//			write_log(10, "count is now %d\n", count);
			tempnode = return_cache_usage_node(this_inode);
			if (tempnode == NULL) {
				write_log(10, "Not found. Alloc a new one\n");
				tmp_size = sizeof(CACHE_USAGE_NODE);
				tempnode = malloc(tmp_size);
				if (tempnode == NULL) {
					write_log(0, "Out of memory (cache)\n");
					errcode = -ENOMEM;
					break;
				}
				memset(tempnode, 0, tmp_size);
			}
			if (tempnode->last_access_time < tempstat.st_atime)
				tempnode->last_access_time = tempstat.st_atime;

			if (tempnode->last_mod_time < tempstat.st_mtime)
				tempnode->last_mod_time = tempstat.st_mtime;

			tempnode->this_inode = this_inode;

			ret = get_block_dirty_status(thisblockpath, NULL,
					&is_dirty);
			if (ret < 0) {
				errcode = errno;
				free(tempnode);
				break;
			}
			/*If this is dirty cache entry*/
			block_size_blk = tempstat.st_blocks * 512;
			if (is_dirty == TRUE)
				tempnode->dirty_cache_size += block_size_blk;
			else
				tempnode->clean_cache_size += block_size_blk;

			write_log(10, "Inserting the node\n");
//			write_log(10, "count is now %d\n", count);
			insert_cache_usage_node(this_inode, tempnode);
			tempnode = NULL;
			errno = 0; de = readdir(dirptr);
			if (de == NULL && errno) {
				errcode = errno;
				break;
			}
//			write_log(10, "count is now %d\n", count);
		}

		if (errcode > 0) {
			write_log(0, "Error in cache replacement. Skipping\n");
			write_log(2, "Code %d, %s\n", errcode,
				strerror(errcode));
		}

		closedir(dirptr);
		if (hcfs_system->system_going_down == TRUE)
			break;
	}
	/* TODO: Perhaps could merge linked lists from all hash entries */
	return 0;
}
