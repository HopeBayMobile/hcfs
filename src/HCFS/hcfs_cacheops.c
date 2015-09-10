/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hfuse_cacheops.c
* Abstract: The c source code file for cache management operations.
*
* Revision History
* 2015/2/11~12 Jiahong added header for this file, and revising coding style.
* 2015/6/3 Jiahong added error handling
*
**************************************************************************/

#include "hcfs_cacheops.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/mman.h>

#include "hcfs_cachebuild.h"
#include "params.h"
#include "fuseop.h"
#include "super_block.h"
#include "global.h"
#include "hfuse_system.h"
#include "logger.h"
#include "macro.h"
#include "metaops.h"
#include "utils.h"

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE

extern SYSTEM_CONF_STRUCT system_config;

/* TODO: Consider whether need to update block status when throwing
out blocks and sync to cloud, and how this may interact with meta
sync in upload process */

/* Helper function for removing local cached block for blocks that
has been synced to backend already */
int _remove_synced_block(ino_t this_inode, struct timeval *builttime,
							long *seconds_slept)
{
	SUPER_BLOCK_ENTRY tempentry;
	char thismetapath[METAPATHLEN];
	FILE *metafptr;
	long long current_block;
	long long total_blocks;
	struct stat temphead_stat;
	struct stat tempstat;
	FILE_META_TYPE temphead;
	long long pagepos;
	char thisblockpath[400];
	BLOCK_ENTRY_PAGE temppage;
	int page_index;
	long long timediff;
	struct timeval currenttime;
	BLOCK_ENTRY *blk_entry_ptr;
	int ret, errcode;
	size_t ret_size;

	ret = super_block_read(this_inode, &tempentry);

	if (ret < 0)
		return ret;

	/* If inode is not dirty or in transit, or if cache is
	already full, check if can replace uploaded blocks */

	/*TODO: if hard limit not reached, perhaps should not
	throw out blocks so aggressively and can sleep for a
	while*/
	if ((tempentry.inode_stat.st_ino > 0) &&
			(S_ISREG(tempentry.inode_stat.st_mode))) {
		ret = fetch_meta_path(thismetapath, this_inode);
		if (ret < 0)
			return ret;

		metafptr = fopen(thismetapath, "r+");
		if (metafptr == NULL) {
			errcode = errno;
			write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
			return -errcode;
		}

		setbuf(metafptr, NULL);
		flock(fileno(metafptr), LOCK_EX);
		if (access(thismetapath, F_OK) < 0) {
			/*If meta file does not exist or error,
			do nothing*/
			errcode = errno;
			if (errcode != ENOENT)
				write_log(0, "IO error in %s. Code %d, %s\n",
					__func__, errcode, strerror(errcode));
			flock(fileno(metafptr), LOCK_UN);
			fclose(metafptr);
			return -errcode;
		}

		current_block = 0;

		FREAD(&temphead_stat, sizeof(struct stat), 1, metafptr);

		FREAD(&temphead, sizeof(FILE_META_TYPE), 1, metafptr);
		total_blocks = (temphead_stat.st_size +
					(MAX_BLOCK_SIZE - 1)) / MAX_BLOCK_SIZE;

		page_index = MAX_BLOCK_ENTRIES_PER_PAGE;

		for (current_block = 0; current_block < total_blocks;
							current_block++) {
			if (page_index >= MAX_BLOCK_ENTRIES_PER_PAGE) {
				pagepos = seek_page2(&temphead, metafptr,
					current_block / BLK_INCREMENTS, 0);

				if (pagepos < 0) {
					errcode = pagepos;
					goto errcode_handle;
				}
				/* No block for this page. Skipping the entire
				page */
				if (pagepos == 0) {
					current_block += (BLK_INCREMENTS-1);
					continue;
				}
				FSEEK(metafptr, pagepos, SEEK_SET);
				FREAD(&temppage,
					sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
				if (ret_size < 1)
					break;

				page_index = 0;
			}

			blk_entry_ptr = &(temppage.block_entries[page_index]);
			if (blk_entry_ptr->status == ST_BOTH) {
				/*Only delete blocks that exists on both
					cloud and local*/
				blk_entry_ptr->status = ST_CLOUD;
				write_log(10,
					"Debug status changed to ST_CLOUD, block %lld, inode %lld\n",
						current_block, this_inode);
				FSEEK(metafptr, pagepos, SEEK_SET);
				FWRITE(&temppage,
					sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
				if (ret_size < 1)
					break;
				ret = fetch_block_path(thisblockpath,
						this_inode, current_block);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}
				ret = stat(thisblockpath, &tempstat);
				if (ret < 0) {
					errcode = errno;
					write_log(0,
						"IO error in %s. Code %d, %s\n",
						__func__, errcode,
						strerror(errcode));
					errcode = -errcode;
					goto errcode_handle;
				}
				sem_wait(&(hcfs_system->access_sem));
				hcfs_system->systemdata.cache_size -=
							tempstat.st_size;
				hcfs_system->systemdata.cache_blocks--;
				ret = unlink(thisblockpath);
				if (ret < 0) {
					errcode = errno;
					write_log(0,
						"IO error in %s. Code %d, %s\n",
						__func__, errcode,
						strerror(errcode));
					errcode = -errcode;
					sem_post(&(hcfs_system->access_sem));
					goto errcode_handle;
				}
				sync_hcfs_system_data(FALSE);
				sem_post(&(hcfs_system->access_sem));
				ret = super_block_mark_dirty(this_inode);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}
			}
/*Adding a delta threshold to avoid thrashing at hard limit boundary*/
			if (hcfs_system->systemdata.cache_size <
					(CACHE_HARD_LIMIT - CACHE_DELTA))
				notify_sleep_on_cache();

			if (hcfs_system->systemdata.cache_size <
						CACHE_SOFT_LIMIT) {
				flock(fileno(metafptr), LOCK_UN);

				while (hcfs_system->systemdata.cache_size <
							CACHE_SOFT_LIMIT) {
					gettimeofday(&currenttime, NULL);
					timediff = currenttime.tv_sec -
							builttime->tv_sec;
					if (timediff < 0)
						timediff = 0;
					/*Rebuild cache usage every five
					minutes if cache usage not near full*/
					if ((timediff > 300) ||
						((*seconds_slept) > 300))
						break;
					sleep(1);
					(*seconds_slept)++;
					if (hcfs_system->system_going_down
						== TRUE)
						break;
				}
				if (hcfs_system->system_going_down == TRUE)
					break;

				gettimeofday(&currenttime, NULL);
				timediff = currenttime.tv_sec -
							builttime->tv_sec;

				if ((hcfs_system->systemdata.cache_size <
							CACHE_SOFT_LIMIT) &&
					((timediff > 300) ||
						((*seconds_slept) > 300)))
					break;

				flock(fileno(metafptr), LOCK_EX);

				/*If meta file does not exist, do nothing*/
				if (access(thismetapath, F_OK) < 0)
					break;

				FSEEK(metafptr, pagepos, SEEK_SET);
				FREAD(&temppage,
					sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
				if (ret_size < 1)
					break;
			}
			if (hcfs_system->system_going_down == TRUE)
				break;
			page_index++;
		}

		flock(fileno(metafptr), LOCK_UN);
		fclose(metafptr);
	}
	return 0;

errcode_handle:
	flock(fileno(metafptr), LOCK_UN);
	fclose(metafptr);
	return errcode;
}

/*TODO: For scanning caches, only need to check one block subfolder a time,
and scan for mtime greater than the last update time for uploads, and scan
for atime for cache replacement*/

/*TODO: Now pick victims with small inode number. Will need to implement
	something smarter.*/
/*Only kick the blocks that's stored on cloud, i.e., stored_where ==ST_BOTH*/
/* TODO: Something better for checking if the inode have cache to be kicked
out. Will need to consider whether to force checking of replacement? */
/************************************************************************
*
* Function name: run_cache_loop
*        Inputs: None
*       Summary: Main loop for scanning for cache usage and remove cached
*                blocks from local disk if synced to backend and if total
*                cache size exceeds some threshold.
*  Return value: None
*
*************************************************************************/
void run_cache_loop(void)
{
	ino_t this_inode;
	struct timeval builttime, currenttime;
	long seconds_slept;
	int e_index;
	char skip_recent, do_something;
	time_t node_time;
	CACHE_USAGE_NODE *this_cache_node;
	int ret;

	ret = -1;
	while (ret < 0) {
		ret = build_cache_usage();
		if (ret < 0) {
			write_log(0, "Error in cache mgmt.\n");
			sleep(10);
		}
	}
	gettimeofday(&builttime, NULL);

	/*Index for doing the round robin in cache dropping*/
	e_index = 0;
	skip_recent = TRUE;
	do_something = FALSE;

	while (hcfs_system->system_going_down == FALSE) {
		seconds_slept = 0;

		while (hcfs_system->systemdata.cache_size >= CACHE_SOFT_LIMIT) {
			if (nonempty_cache_hash_entries <= 0) {
				/* All empty */
				ret = build_cache_usage();
				if (ret < 0) {
					write_log(0, "Error in cache mgmt.\n");
					sleep(10);
					continue;
				}
				gettimeofday(&builttime, NULL);
				e_index = 0;
				skip_recent = TRUE;
				do_something = FALSE;
			}

			/* End of hash table. Restart at index 0 */
			if (e_index >= CACHE_USAGE_NUM_ENTRIES) {
				if ((do_something == FALSE) &&
						(skip_recent == FALSE)) {
					ret = build_cache_usage();
					if (ret < 0) {
						write_log(0,
							"Error in cache mgmt.\n");
						sleep(10);
						continue;
					}
					gettimeofday(&builttime, NULL);
					e_index = 0;
					skip_recent = TRUE;
					do_something = FALSE;
				} else {
					if ((do_something == FALSE) &&
							(skip_recent == TRUE))
						skip_recent = FALSE;
					e_index = 0;
					do_something = FALSE;
				}
			}

			/* skip empty bucket */
			if (inode_cache_usage_hash[e_index] == NULL) {
				e_index++;
				continue;
			}

			/* All dirty. Local data cannot be deleted. */
			if (inode_cache_usage_hash[e_index]->clean_cache_size
									<= 0) {
				e_index++;
				continue;
			}

			if (skip_recent == TRUE) {
				gettimeofday(&currenttime, NULL);
				node_time = inode_cache_usage_hash[e_index]->
							last_access_time;
				if (node_time <
					inode_cache_usage_hash[e_index]->
								last_mod_time)
					node_time =
						inode_cache_usage_hash[e_index]
							->last_mod_time;
				if ((currenttime.tv_sec - node_time) < 300) {
					e_index++;
					continue;
				}
			}
			do_something = TRUE;

			this_inode =
				inode_cache_usage_hash[e_index]->this_inode;

			this_cache_node =
				return_cache_usage_node(
				inode_cache_usage_hash[e_index]->this_inode);

			if (this_cache_node)
				free(this_cache_node);
			e_index++;

			ret = _remove_synced_block(this_inode, &builttime,
								&seconds_slept);
			if (ret < 0)
				sleep(10);
		}
		if (hcfs_system->system_going_down == TRUE)
			break;

		while (hcfs_system->systemdata.cache_size < CACHE_SOFT_LIMIT) {
			gettimeofday(&currenttime, NULL);
			/*Rebuild cache usage every five minutes if cache usage
			not near full*/
			if (((currenttime.tv_sec-builttime.tv_sec) > 300) ||
							(seconds_slept > 300)) {
				ret = build_cache_usage();
				if (ret < 0) {
					write_log(0, "Error in cache mgmt.\n");
					sleep(10);
					continue;
				}
				gettimeofday(&builttime, NULL);
				seconds_slept = 0;
				e_index = 0;
				skip_recent = TRUE;
				do_something = FALSE;
			}
			sleep(1);
			if (hcfs_system->system_going_down == TRUE)
				break;
			seconds_slept++;
		}
	}
}

/************************************************************************
*
* Function name: sleep_on_cache_full
*        Inputs: None
*       Summary: Routine for sleeping threads/processes on cache full.
*                caller will wait until being notified by
*                notify_sleep_on_cache.
*  Return value: None
*
*************************************************************************/
void sleep_on_cache_full(void)
{
	sem_post(&(hcfs_system->num_cache_sleep_sem));
	sem_wait(&(hcfs_system->check_cache_sem));
	sem_wait(&(hcfs_system->num_cache_sleep_sem));
	sem_post(&(hcfs_system->check_next_sem));
}

/************************************************************************
*
* Function name: sleep_on_cache_full
*        Inputs: None
*       Summary: Routine for waking threads/processes sleeping using
*                sleep_on_cache_full when cache not full.
*  Return value: None
*
*************************************************************************/
void notify_sleep_on_cache(void)
{
	int num_cache_sleep_sem_value;

	while (TRUE) {
		sem_getvalue(&(hcfs_system->num_cache_sleep_sem),
					&num_cache_sleep_sem_value);

		/*If still have threads/processes waiting on cache not full*/
		if (num_cache_sleep_sem_value > 0) {
			sem_post(&(hcfs_system->check_cache_sem));
			sem_wait(&(hcfs_system->check_next_sem));
		} else {
			break;
		}
	}
}

