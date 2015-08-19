/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hfuse_cachebuild.c
* Abstract: The c source code file for building structure for cache usage.
*
* Revision History
* 2015/2/11 Jiahong added header for this file, and revising coding style.
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
#include <attr/xattr.h>
#include <sys/mman.h>

#include "params.h"
#include "fuseop.h"
#include "super_block.h"
#include "logger.h"
#include "global.h"

extern SYSTEM_CONF_STRUCT system_config;

/************************************************************************
*
* Function name: cache_usage_hash_init
*        Inputs: None
*       Summary: Initialize cache usage structure.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int cache_usage_hash_init(void)
{
	int count;
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
	int hash_value;
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
	int hash_value;
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
int compare_cache_usage(CACHE_USAGE_NODE *first_node,
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
int build_cache_usage(void)
{
	char blockpath[400];
	char thisblockpath[400];
	DIR *dirptr;
	int count;
	struct dirent temp_dirent;
	struct dirent *direntptr;
	int ret, errcode;
	long long blockno;
	ino_t this_inode;
	struct stat tempstat;
	CACHE_USAGE_NODE *tempnode;
	char tempval[10];
	size_t tmp_size;

	write_log(5, "Building cache usage hash table\n");
	ret = cache_usage_hash_init();
	if (ret < 0)
		return ret;

	for (count = 0; count < NUMSUBDIR; count++) {
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

		ret = readdir_r(dirptr, &temp_dirent, &direntptr);
		if (ret > 0) {
			errcode = ret;
			write_log(0, "Error in cache replacement. Skipping\n");
			write_log(2, "Code %d, %s\n", errcode,
				strerror(errcode));
			closedir(dirptr);
			continue;
		}

		while (direntptr != NULL) {
			if (hcfs_system->system_going_down == TRUE)
				break;
			errcode = 0;
#ifdef ARM_32bit_
			ret = sscanf(temp_dirent.d_name, "block%lld_%lld",
                                                        &this_inode, &blockno);
#else
			ret = sscanf(temp_dirent.d_name, "block%ld_%lld",
							&this_inode, &blockno);
#endif
			if (ret != 2) {
				ret = readdir_r(dirptr, &temp_dirent,
					&direntptr);
				if (ret > 0) {
					errcode = ret;
					break;
				}
				continue;
			}
			ret = fetch_block_path(thisblockpath, this_inode, blockno);
			if (ret < 0) {
				errcode = ret;
				break;
			}
			ret = stat(thisblockpath, &tempstat);

			if (ret != 0) {
				ret = readdir_r(dirptr, &temp_dirent,
					&direntptr);
				if (ret > 0) {
					errcode = ret;
					break;
				}
				continue;
			}

			tempnode = return_cache_usage_node(this_inode);
			if (tempnode == NULL) {
				tmp_size = sizeof(CACHE_USAGE_NODE);
				tempnode = malloc(tmp_size);
				memset(tempnode, 0, tmp_size);
			}
			if (tempnode->last_access_time < tempstat.st_atime)
				tempnode->last_access_time = tempstat.st_atime;

			if (tempnode->last_mod_time < tempstat.st_mtime)
				tempnode->last_mod_time = tempstat.st_mtime;

			tempnode->this_inode = this_inode;

			ret = getxattr(thisblockpath, "user.dirty",
							(void *)tempval, 1);
			if (ret < 0) {
				errcode = errno;
				break;
			}
			/*If this is dirty cache entry*/
			if (!strncmp(tempval, "T", 1)) {
				tempnode->dirty_cache_size += tempstat.st_size;
			} else {
				/*If clean cache entry*/
				if (!strncmp(tempval, "F", 1))
					tempnode->clean_cache_size +=
							tempstat.st_size;
				/*Otherwise, don't know the status of
						the block, so do nothing*/
			}
			insert_cache_usage_node(this_inode, tempnode);
			ret = readdir_r(dirptr, &temp_dirent, &direntptr);
			if (ret > 0) {
				errcode = ret;
				break;
			}
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
	return 0;
}