/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hfuse_cacheops.c
* Abstract: The c source code file for cache management operations.
*
* Revision History
* 2015/2/11~12 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/

#include "hcfs_cacheops.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <attr/xattr.h>
#include <sys/mman.h>

#include "hcfs_cachebuild.h"
#include "params.h"
#include "fuseop.h"
#include "super_block.h"
#include "global.h"
#include "hfuse_system.h"

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE

extern SYSTEM_CONF_STRUCT system_config;

/* Helper function for removing local cached block for blocks that
are synced to backend already */
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
	size_t ret_val;
	int page_index;
	long long timediff;
	struct timeval currenttime;
	BLOCK_ENTRY *blk_entry_ptr;

	super_block_read(this_inode, &tempentry);

	/* If inode is not dirty or in transit, or if cache is
	already full, check if can replace uploaded blocks */

	/*TODO: if hard limit not reached, perhaps should not
	throw out blocks so aggressively and can sleep for a
	while*/
	if ((tempentry.inode_stat.st_ino > 0) &&
			(tempentry.inode_stat.st_mode & S_IFREG)) {
		fetch_meta_path(thismetapath, this_inode);
		metafptr = fopen(thismetapath, "r+");
		if (metafptr == NULL)
			return -1;

		setbuf(metafptr, NULL);
		flock(fileno(metafptr), LOCK_EX);
		if (access(thismetapath, F_OK) < 0) {
			/*If meta file does not exist,
			do nothing*/
			flock(fileno(metafptr), LOCK_UN);
			fclose(metafptr);
			return -1;
		}

		current_block = 0;

		fread(&temphead_stat, sizeof(struct stat), 1, metafptr);

		fread(&temphead, sizeof(FILE_META_TYPE), 1, metafptr);
		total_blocks = (temphead_stat.st_size +
					(MAX_BLOCK_SIZE - 1)) / MAX_BLOCK_SIZE;

		page_index = MAX_BLOCK_ENTRIES_PER_PAGE;

		for (current_block = 0; current_block < total_blocks;
							current_block++) {
			if (page_index >= MAX_BLOCK_ENTRIES_PER_PAGE) {
				pagepos = seek_page2(&temphead, metafptr,
					current_block / BLK_INCREMENTS, 0);

				/* No block for this page. Skipping the entire
				page */
				if (pagepos == 0) {
					current_block += (BLK_INCREMENTS-1);
					continue;
				}
				fseek(metafptr, pagepos, SEEK_SET);
				ret_val = fread(&temppage,
					sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
				if (ret_val < 1)
					break;

				page_index = 0;
			}

			blk_entry_ptr = &(temppage.block_entries[page_index]);
			if (blk_entry_ptr->status == ST_BOTH) {
				/*Only delete blocks that exists on both
					cloud and local*/
				blk_entry_ptr->status = ST_CLOUD;

				printf("Debug status changed to ST_CLOUD, block %lld, inode %lld\n",
						current_block, this_inode);
				fseek(metafptr, pagepos, SEEK_SET);
				ret_val = fwrite(&temppage,
					sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
				if (ret_val < 1)
					break;
				fetch_block_path(thisblockpath, this_inode,
								current_block);

				stat(thisblockpath, &tempstat);
				sem_wait(&(hcfs_system->access_sem));
				hcfs_system->systemdata.cache_size -=
							tempstat.st_size;
				hcfs_system->systemdata.cache_blocks--;
				unlink(thisblockpath);
				sync_hcfs_system_data(FALSE);
				sem_post(&(hcfs_system->access_sem));
				super_block_mark_dirty(this_inode);
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
				}
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

				fseek(metafptr, pagepos, SEEK_SET);
				ret_val = fread(&temppage,
					sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
				if (ret_val < 1)
					break;
			}
			page_index++;
		}

		flock(fileno(metafptr), LOCK_UN);
		fclose(metafptr);
	}
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
void run_cache_loop(void)
{
	ino_t this_inode;
	int ret_val;
	struct timeval builttime, currenttime;
	long seconds_slept;
	int e_index;
	char skip_recent, do_something;
	time_t node_time;
	CACHE_USAGE_NODE *this_cache_node;

	build_cache_usage();
	gettimeofday(&builttime, NULL);

	/*Index for doing the round robin in cache dropping*/
	e_index = 0;
	skip_recent = TRUE;
	do_something = FALSE;

	while (TRUE) {
		seconds_slept = 0;

		while (hcfs_system->systemdata.cache_size >= CACHE_SOFT_LIMIT) {
			if (nonempty_cache_hash_entries <= 0) {
				build_cache_usage();
				gettimeofday(&builttime, NULL);
				e_index = 0;
				skip_recent = TRUE;
				do_something = FALSE;
			}

			if (e_index >= CACHE_USAGE_NUM_ENTRIES) {
				if ((do_something == FALSE) &&
						(skip_recent == FALSE)) {
					build_cache_usage();
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

			if (inode_cache_usage_hash[e_index] == NULL) {
				e_index++;
				continue;
			}

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

			free(this_cache_node);
			e_index++;

			ret_val = _remove_synced_block(this_inode, &builttime,
								&seconds_slept);
		}

		while (hcfs_system->systemdata.cache_size < CACHE_SOFT_LIMIT) {
			gettimeofday(&currenttime, NULL);
			/*Rebuild cache usage every five minutes if cache usage
			not near full*/
			if (((currenttime.tv_sec-builttime.tv_sec) > 300) ||
							(seconds_slept > 300)) {
				build_cache_usage();
				gettimeofday(&builttime, NULL);
				seconds_slept = 0;
				e_index = 0;
				skip_recent = TRUE;
				do_something = FALSE;
			}
			sleep(1);
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

