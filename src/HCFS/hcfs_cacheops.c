/*************************************************************************
*
* Copyright © 2014-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hfuse_cacheops.c
* Abstract: The c source code file for cache management operations.
*
* Revision History
* 2015/2/11~12 Jiahong added header for this file, and revising coding style.
* 2015/6/3 Jiahong added error handling
* 2015/10/22 Kewei added mechanism skipping pinned inodes.
* 2016/6/7 Jiahong changing code for recovering mode
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
#include <inttypes.h>

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
#include "rebuild_super_block.h"

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE

/* TODO: Consider whether need to update block status when throwing
out blocks and sync to cloud, and how this may interact with meta
sync in upload process */

/*
 * Helper function for removing local cached block for blocks that
 * has been synced to backend already.
 *
 * @param this_inode The inode to be removed
 * @param builttime Builttime of last cache usage.
 * @param seconds_slept Total sleeping time accumulating up to now.
 *
 * @return 0 on succeeding in removing the inode, 1 on skipping the inode,
 *         otherwise negative error code.
 */
int32_t _remove_synced_block(ino_t this_inode, struct timeval *builttime,
							int64_t *seconds_slept)
{
	SUPER_BLOCK_ENTRY tempentry;
	char thismetapath[METAPATHLEN];
	FILE *metafptr;
	int64_t current_block;
	int64_t total_blocks;
	int64_t block_size_blk;
	HCFS_STAT temphead_stat;
	struct stat block_stat; /* block ops */
	FILE_META_TYPE temphead;
	int64_t pagepos;
	char thisblockpath[400];
	BLOCK_ENTRY_PAGE temppage;
	int32_t page_index;
	int64_t timediff;
	struct timeval currenttime;
	BLOCK_ENTRY *blk_entry_ptr;
	int32_t ret, errcode, semval;
	size_t ret_size;
	sem_t *semptr;

	write_log(10, "Begin remove sync block inode %" PRIu64 "\n",
	          (uint64_t)this_inode);

	/* Try fetching meta file from backend if in restoring mode */
	if (hcfs_system->system_restoring == RESTORING_STAGE2) {
		ret = restore_meta_super_block_entry(this_inode,
		                                     &(tempentry.inode_stat));
		if (ret < 0)
			return ret;
	} else {

		ret = super_block_read(this_inode, &tempentry);

		if (ret < 0)
			return ret;
	}

	/* If inode is not dirty or in transit, or if cache is
	already full, check if can replace uploaded blocks */

	/*TODO: if hard limit not reached, perhaps should not
	throw out blocks so aggressively and can sleep for a
	while*/
	if ((tempentry.inode_stat.ino > 0) &&
			(S_ISREG(tempentry.inode_stat.mode))) {
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

		FREAD(&temphead_stat, sizeof(HCFS_STAT), 1, metafptr);
		FREAD(&temphead, sizeof(FILE_META_TYPE), 1, metafptr);

		/* Skip if inode is pinned */
		if (P_IS_PIN(temphead.local_pin)) {
			write_log(10, "Debug: inode %"PRIu64" is pinned. "
				"Skip to page it out.\n", (uint64_t)this_inode);
			flock(fileno(metafptr), LOCK_UN);
			fclose(metafptr);
			return 1;
		}

		total_blocks =
		    BLOCKS_OF_SIZE(temphead_stat.size, MAX_BLOCK_SIZE);

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
				ret_size = FREAD(&temppage,
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
				/* Increase the counter for the number of times
				this block is paged out. Reset to zero if
				overflow */
				blk_entry_ptr->paged_out_count++;
				if (blk_entry_ptr->paged_out_count >
				    (UINT32_MAX - (uint32_t) 10))
					blk_entry_ptr->paged_out_count = 0;
				write_log(10,
					"Debug status changed to ST_CLOUD, block %lld, inode %lld\n",
						current_block, this_inode);
				FSEEK(metafptr, pagepos, SEEK_SET);
				ret_size = FWRITE(&temppage,
					sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
				if (ret_size < 1)
					break;
				ret = fetch_block_path(thisblockpath,
						this_inode, current_block);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}
				ret = stat(thisblockpath, &block_stat);
				if (ret < 0) {
					errcode = errno;
					write_log(0,
						"IO error in %s. Code %d, %s\n",
						__func__, errcode,
						strerror(errcode));
					errcode = -errcode;
					goto errcode_handle;
				}
				block_size_blk = block_stat.st_blocks * 512;
				change_system_meta(0, 0, -block_size_blk,
						-1, 0, 0, TRUE);
				ret = unlink(thisblockpath);
				if (ret < 0) {
					errcode = errno;
					write_log(0,
						"IO error in %s. Code %d, %s\n",
						__func__, errcode,
						strerror(errcode));
					errcode = -errcode;
					goto errcode_handle;
				}
				ret = update_file_stats(metafptr, 0, -1,
							-block_size_blk,
							0, this_inode);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}

				/* Do not sync the block status change
				due to paging out */
				/*
				ret = super_block_mark_dirty(this_inode);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}
				*/
			}
/*Adding a delta threshold to avoid thrashing at hard limit boundary*/
			if (hcfs_system->systemdata.cache_size <
					(CACHE_HARD_LIMIT - CACHE_DELTA))
				notify_sleep_on_cache(0);

			/* If cache size < soft limit, take a break and wait
			for exceeding soft limit. Stop paging out these blocks
			if cache size is still < soft limit after 5 mins.
			Otherwise lock meta and remove next block. */
			if (hcfs_system->systemdata.cache_size <
						CACHE_SOFT_LIMIT) {
				flock(fileno(metafptr), LOCK_UN);

				semptr = &(hcfs_system->num_cache_sleep_sem);
				while (hcfs_system->systemdata.cache_size <
							CACHE_SOFT_LIMIT) {
					gettimeofday(&currenttime, NULL);
					timediff = currenttime.tv_sec -
							builttime->tv_sec;
					if (timediff < 0)
						timediff = 0;
					/*Rebuild cache usage every five
					minutes if cache usage not near full*/
					if ((timediff > SCAN_INT) ||
						((*seconds_slept) > SCAN_INT))
						break;
					sleep(1);
					(*seconds_slept)++;
					if (hcfs_system->system_going_down
						== TRUE)
						break;
					/* Check if some thread is still
					sleeping */
					ret = sem_getvalue(semptr, &semval);
					if ((ret == 0) && (semval > 0))
						notify_sleep_on_cache(0);
				}
				if (hcfs_system->system_going_down == TRUE)
					break;

				gettimeofday(&currenttime, NULL);
				timediff = currenttime.tv_sec -
							builttime->tv_sec;

				if ((hcfs_system->systemdata.cache_size <
							CACHE_SOFT_LIMIT) &&
					((timediff > SCAN_INT) ||
						((*seconds_slept) > SCAN_INT)))
					break;

				flock(fileno(metafptr), LOCK_EX);

				/* Check pin status before paging blocks out */
				FSEEK(metafptr, sizeof(HCFS_STAT), SEEK_SET);
				FREAD(&temphead, sizeof(FILE_META_TYPE),
					1, metafptr);
				if (P_IS_PIN(temphead.local_pin)) {
					write_log(10, "Debug: inode %"PRIu64" "
						"is pinned when paging it out. "
						"Stop\n", (uint64_t)this_inode);
					break;
				}

				/*If meta file does not exist, do nothing*/
				if (access(thismetapath, F_OK) < 0)
					break;

				FSEEK(metafptr, pagepos, SEEK_SET);
				ret_size = FREAD(&temppage,
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


static int32_t _check_cache_replace_result(int64_t *num_removed_inode)
{
	/* If number of removed inodes = 0, and cache size is full, and
	 * backend is offline, then wake them up and tell them cannot
	 * do this action.
	 */
	if (*num_removed_inode == 0) { /* No inodes be removed */
		if ((hcfs_system->systemdata.cache_size >=
			CACHE_HARD_LIMIT - CACHE_DELTA) &&
			(hcfs_system->sync_paused)) {
			/* Wake them up and tell them cannot do this action */
			write_log(4, "Cache size exceeds threshold, "
				"but nothing can be paged out\n");
			notify_sleep_on_cache(-EIO);
			sleep(2);
			/* Try again after 2 seconds just in case some thread
			slipped by status changes
			before we wait on something_to_replace*/
			notify_sleep_on_cache(-EIO);
		}
		/* If in the previous round no replace is done,
		need to sleep until cache replacement is
		possible */
		sem_wait(&(hcfs_system->something_to_replace));
		sem_post(&(hcfs_system->something_to_replace));
	}

	*num_removed_inode = 0;
	return 0;
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
#ifdef _ANDROID_ENV_
void *run_cache_loop(void *ptr)
#else
void run_cache_loop(void)
#endif
{
	ino_t this_inode;
	struct timeval builttime, currenttime;
	int64_t seconds_slept;
	int32_t e_index;
	char skip_recent, do_something;
	time_t node_time;
	CACHE_USAGE_NODE *this_cache_node;
	int32_t ret, semval;
	int64_t num_removed_inode;
	sem_t *semptr;

#ifdef _ANDROID_ENV_
	UNUSED(ptr);
#endif
	ret = -1;
	while (ret < 0) {
		if (hcfs_system->system_going_down == TRUE)
			break;
		num_removed_inode = 0;
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
		write_log(10, "Start one round of cache replacement\n");
		write_log(10, "Cache size is %lld\n",
		          hcfs_system->systemdata.cache_size);
		seconds_slept = 0;

		while (hcfs_system->systemdata.cache_size >= CACHE_SOFT_LIMIT) {
			if (hcfs_system->system_going_down == TRUE)
				break;

			write_log(10, "Need to throw out something\n");
			if (nonempty_cache_hash_entries <= 0) {
				/* All empty */
				write_log(10, "Recomputing cache usage\n");
				_check_cache_replace_result(&num_removed_inode);
				do_something = FALSE;

				ret = build_cache_usage();
				if (ret < 0) {
					write_log(0, "Error in cache mgmt.\n");
					sleep(10);
					continue;
				}
				gettimeofday(&builttime, NULL);
				e_index = 0;
				skip_recent = TRUE;
			}

			/* End of hash table. Restart at index 0 */
			if (e_index >= CACHE_USAGE_NUM_ENTRIES) {
				if ((do_something == FALSE) &&
						(skip_recent == FALSE)) {
					write_log(10, "Recomputing cache usage part 2\n");
					_check_cache_replace_result(&num_removed_inode);
					do_something = FALSE;
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
			//	write_log(10, "Skipping, part 1\n");
				continue;
			}

			/* All dirty. Local data cannot be deleted. */
			if (inode_cache_usage_hash[e_index]->clean_cache_size
									<= 0) {
				e_index++;
			//	write_log(10, "Skipping, part 2\n");
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
				if ((currenttime.tv_sec - node_time) <
				     SCAN_INT) {
					e_index++;
			//		write_log(10, "Skipping, part 3\n");
					continue;
				}
			}
			do_something = TRUE;

			this_inode =
				inode_cache_usage_hash[e_index]->this_inode;
			write_log(10, "Preparing to remove blocks in %" PRIu64 "\n",
			          (uint64_t)this_inode);

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
			else if (ret == 0)
				num_removed_inode++;
		}
		if (hcfs_system->system_going_down == TRUE)
			break;

		semptr = &(hcfs_system->something_to_replace);

		while (hcfs_system->systemdata.cache_size < CACHE_SOFT_LIMIT) {
			gettimeofday(&currenttime, NULL);
			/*Rebuild cache usage every five minutes if cache usage
			not near full*/
			write_log(10, "Checking cache size %lld, %lld\n",
			          hcfs_system->systemdata.cache_size,
			          CACHE_SOFT_LIMIT);

			if (((currenttime.tv_sec-builttime.tv_sec) >
			      SCAN_INT) ||
			     (seconds_slept > SCAN_INT)) {
				semval = 0;
				ret = sem_getvalue(semptr, &semval);
				if ((ret == 0) && (semval == 0)) {
					seconds_slept = 0;
					gettimeofday(&builttime, NULL);
				}
			}

			if (((currenttime.tv_sec-builttime.tv_sec) >
			      SCAN_INT) ||
			     (seconds_slept > SCAN_INT)) {
				ret = build_cache_usage();
				if (ret < 0) {
					write_log(0, "Error in cache mgmt.\n");
					sleep(10);
					continue;
				}
				num_removed_inode = 0;
				gettimeofday(&builttime, NULL);
				seconds_slept = 0;
				e_index = 0;
				skip_recent = TRUE;
				do_something = FALSE;
			}
			sleep(1);

			if (hcfs_system->system_going_down == TRUE)
				break;

			/* Check if some thread is still sleeping */
			ret = sem_getvalue(&(hcfs_system->num_cache_sleep_sem),
			                   &semval);
			if ((ret == 0) && (semval > 0))
				notify_sleep_on_cache(0);

			seconds_slept++;
		}
	}
	notify_sleep_on_cache(-ESHUTDOWN);
#ifdef _ANDROID_ENV_
	return NULL;
#endif
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
int32_t sleep_on_cache_full(void)
{
	int32_t cache_replace_status;
	int32_t num_replace;

	/* Check cache replacement status */
	cache_replace_status = hcfs_system->systemdata.cache_replace_status;
	if (cache_replace_status < 0) {
		return cache_replace_status;
	} else {
		sem_getvalue(&(hcfs_system->something_to_replace),
				&num_replace);
		/* In case of network disconn and nothing can be replaced,
		 * ask cache manager to traverse all blocks one more time
		 * and set cache status again. */
		if (hcfs_system->sync_paused == TRUE &&
				num_replace == 0)
			sem_post(&(hcfs_system->something_to_replace));
	}

	/* Check if sync is paused, and wake it up if needed */
	int32_t sem_val = 0;
	sem_check_and_release(&(hcfs_system->sync_control_sem), &sem_val);

	sem_post(&(hcfs_system->num_cache_sleep_sem)); /* Count++ */
	sem_wait(&(hcfs_system->check_cache_sem)); /* Sleep a while */
	sem_wait(&(hcfs_system->num_cache_sleep_sem)); /* Count-- */
	cache_replace_status = hcfs_system->systemdata.cache_replace_status;
	sem_post(&(hcfs_system->check_cache_replace_status_sem)); /*Get status*/
	sem_post(&(hcfs_system->check_next_sem));

	return cache_replace_status;
}

/************************************************************************
*
* Function name: notify_sleep_on_cache
*        Inputs: int32_t cache_replace_status
*       Summary: Routine for waking threads/processes sleeping from using
*                sleep_on_cache_full.
*  Return value: None
*
*************************************************************************/
void notify_sleep_on_cache(int32_t cache_replace_status)
{
	int32_t num_cache_sleep_sem_value;

	sem_wait(&(hcfs_system->access_sem));
	hcfs_system->systemdata.cache_replace_status = cache_replace_status;
	sem_post(&(hcfs_system->access_sem));

	while (TRUE) {
		sem_getvalue(&(hcfs_system->num_cache_sleep_sem),
					&num_cache_sleep_sem_value);

		/*If still have threads/processes waiting on cache full*/
		if (num_cache_sleep_sem_value > 0) {
			sem_post(&(hcfs_system->check_cache_sem)); /* Wake up */
			sem_wait(&(hcfs_system->check_cache_replace_status_sem));
			sem_wait(&(hcfs_system->check_next_sem));
		} else {
			break;
		}
	}
}

/************************************************************************
*
* Function name: get_cache_limit
*        Inputs: const char pin_type
*       Summary: Returns cache threshold for (pin_type)
*  Return value: Cache space limit if successful. Otherwise returns the
*  		 negation of linux error code.
*
*************************************************************************/
int64_t get_cache_limit(const char pin_type)
{
	if (pin_type < NUM_PIN_TYPES)
		return CACHE_LIMITS(pin_type);
	else
		return -EINVAL;
}

/************************************************************************
*
* Function name: get_pinned_limit
*        Inputs: const char pin_type
*       Summary: Returns pinned threshold for (pin_type)
*  Return value: Pinned space limit if successful. Otherwise returns the
*  		 negation of linux error code.
*
*************************************************************************/
int64_t get_pinned_limit(const char pin_type)
{
	if (pin_type < NUM_PIN_TYPES)
		return PINNED_LIMITS(pin_type);
	else
		return -EINVAL;
}
