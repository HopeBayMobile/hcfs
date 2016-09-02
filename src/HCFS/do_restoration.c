/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: do_restoration.h
* Abstract: The c source file for restore operations
*
* Revision History
* 2016/7/25 Jiahong created this file.
*
**************************************************************************/

#include "do_restoration.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ftw.h>

#include "utils.h"
#include "macro.h"
#include "fuseop.h"
#include "event_filter.h"
#include "event_notification.h"
#include "hcfs_fromcloud.h"
#include "metaops.h"
#include "mount_manager.h"

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE

/* FEATURE TODO: How to verify that the meta / data stored on cloud
is enough for restoration (what if important system app / files cannot
be restored?) */
/* FEATURE TODO: How to purge files that cannot be restored correctly
from the directory structure */

void init_restore_path(void)
{
	snprintf(RESTORE_METAPATH, METAPATHLEN, "%s_restore",
	         METAPATH);
	snprintf(RESTORE_BLOCKPATH, BLOCKPATHLEN, "%s_restore",
	         BLOCKPATH);
	sem_init(&(restore_sem), 0, 1);
	sem_init(&(backup_pkg_sem), 0, 1);
	have_new_pkgbackup = TRUE;
}

int32_t fetch_restore_stat_path(char *pathname)
{
	snprintf(pathname, METAPATHLEN, "%s/system_restoring_status",
	         METAPATH);
	return 0;
}

int32_t tag_restoration(char *content)
{
	char restore_stat_path[METAPATHLEN];
	char restore_stat_path2[METAPATHLEN];
	FILE *fptr = NULL;
	int32_t ret, errcode;
	size_t ret_size;
	char is_open;

	is_open = FALSE;
	fetch_restore_stat_path(restore_stat_path);
	if (access(restore_stat_path, F_OK) == 0)
		unlink(restore_stat_path);
	fptr = fopen(restore_stat_path, "w");
	if (fptr == NULL) {
		write_log(4, "Unable to determine restore status\n");
		errcode = -EIO;
		goto errcode_handle;
	}
	is_open = TRUE;
	FWRITE(content, 1, strlen(content), fptr);
	fclose(fptr);
	is_open = FALSE;

	snprintf(restore_stat_path2, METAPATHLEN, "%s/system_restoring_status",
	         RESTORE_METAPATH);
	if (access(restore_stat_path2, F_OK) == 0)
		unlink(restore_stat_path2);

	ret = link(restore_stat_path, restore_stat_path2);
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Link error in tagging\n");
		errcode = -errcode;
		goto errcode_handle;
	}
	return 0;
errcode_handle:
	if (is_open)
		fclose(fptr);
	return errcode;
}

BOOL _enough_local_space(void)
{

	/* Need cache size to be less than 0.2 of max possible cache size */
	if (hcfs_system->systemdata.cache_size >=
	    CACHE_HARD_LIMIT * REDUCED_RATIO)
		return FALSE;

	/* Need pin size to be less than 0.2 of max possible pin size */
	if (hcfs_system->systemdata.pinned_size >=
	    MAX_PINNED_LIMIT * REDUCED_RATIO)
		return FALSE;

	/* Need pin size to be less than 0.2 of max possible meta size */
	if (hcfs_system->systemdata.system_meta_size >=
	    META_SPACE_LIMIT * REDUCED_RATIO)
		return FALSE;

	return TRUE;
}

int32_t initiate_restoration(void)
{
	int32_t ret, errcode;

	ret = check_restoration_status();
	if (ret > 0) {
		/* If restoration is already in progress, do not permit */
		return -EPERM;
	}

	sem_wait(&restore_sem);

	/* First check if there is enough space for restoration */
	sem_wait(&(hcfs_system->access_sem));

	if (_enough_local_space() == FALSE) {
		sem_post(&(hcfs_system->access_sem));
		errcode = -ENOSPC;
		goto errcode_handle;
	}

	sem_post(&(hcfs_system->access_sem));

	/* First create the restoration folders if needed */
	if (access(RESTORE_METAPATH, F_OK) != 0)
		MKDIR(RESTORE_METAPATH, 0700);
	if (access(RESTORE_BLOCKPATH, F_OK) != 0)
		MKDIR(RESTORE_BLOCKPATH, 0700);

	/* Tag status of restoration */
	ret = tag_restoration("downloading_minimal");
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	sem_wait(&(hcfs_system->access_sem));
	if (hcfs_system->system_restoring != RESTORING_STAGE1)
		hcfs_system->system_restoring = RESTORING_STAGE1;
	sem_post(&(hcfs_system->access_sem));

	sem_post(&restore_sem);
	return 0;

errcode_handle:
	sem_post(&restore_sem);
	return errcode;
}

int32_t check_restoration_status(void)
{
	char restore_stat_path[METAPATHLEN];
	char restore_stat[100];
	FILE *fptr;
	int32_t errcode, retval;
	size_t ret_size;
	char is_open;

	sem_wait(&restore_sem);

	retval = NOT_RESTORING;
	is_open = FALSE;
	fetch_restore_stat_path(restore_stat_path);
	if (access(restore_stat_path, F_OK) == 0) {
		restore_stat[0] = 0;
		fptr = fopen(restore_stat_path, "r");
		if (fptr == NULL) {
			write_log(4, "Unable to determine restore status\n");
			errcode = -EIO;
			goto errcode_handle;
		}

		is_open = TRUE;
		FREAD(restore_stat, 1, 90, fptr);
		fclose(fptr);
		is_open = FALSE;

		if (strncmp(restore_stat, "downloading_minimal",
		            strlen("downloading_minimal")) == 0) {
			write_log(10, "Restoring: downloading meta\n");
			retval = RESTORING_STAGE1;
		} else if (strncmp(restore_stat, "rebuilding_meta",
		            strlen("rebuilding_meta")) == 0) {
			write_log(10, "Rebuilding meta\n");
			retval = RESTORING_STAGE2;
		}
	}

	sem_post(&restore_sem);

	return retval;
errcode_handle:
	if (is_open) {
		fclose(fptr);
		is_open = FALSE;
	}
	sem_post(&restore_sem);

	return errcode;
}

int32_t notify_restoration_result(int8_t stage, int32_t result)
{
	int32_t ret;
	char msgstr[100];

	switch (stage) {
	case 1:
		/* Restoration stage 1 */
		snprintf(msgstr, 100, "{\"result\":%d}", result);
		ret = add_notify_event(RESTORATION_STAGE1_CALLBACK,
		                       msgstr, TRUE);
		break;
	case 2:
		/* Restoration stage 2 */
		snprintf(msgstr, 100, "{\"result\":%d}", result);
		ret = add_notify_event(RESTORATION_STAGE2_CALLBACK,
		                       msgstr, TRUE);
		break;
	default:
		/* stage should be either 1 or 2 */
		ret = -EINVAL;
		break;
	}

	return ret;
}

int32_t restore_stage1_reduce_cache(void)
{
	sem_wait(&(hcfs_system->access_sem));

	/* Need enough cache space */
	if (_enough_local_space() == FALSE) {
		sem_post(&(hcfs_system->access_sem));
		return -ENOSPC;
	}

	CACHE_HARD_LIMIT = CACHE_HARD_LIMIT * REDUCED_RATIO;
	META_SPACE_LIMIT = META_SPACE_LIMIT * REDUCED_RATIO;

	/* Change the max system size as well */
	hcfs_system->systemdata.system_quota = CACHE_HARD_LIMIT;
	system_config->max_cache_limit[P_UNPIN] = CACHE_HARD_LIMIT;
	system_config->max_pinned_limit[P_UNPIN] = MAX_PINNED_LIMIT;

	system_config->max_cache_limit[P_PIN] = CACHE_HARD_LIMIT;
	system_config->max_pinned_limit[P_PIN] = MAX_PINNED_LIMIT;

	system_config->max_cache_limit[P_HIGH_PRI_PIN] =
		CACHE_HARD_LIMIT + RESERVED_CACHE_SPACE;
	system_config->max_pinned_limit[P_HIGH_PRI_PIN] =
		MAX_PINNED_LIMIT + RESERVED_CACHE_SPACE;

	sem_post(&(hcfs_system->access_sem));

	return 0;
}

int32_t _check_and_create_restorepaths(void)
{
	char tempname[METAPATHLEN];
	int32_t sub_dir;
	int32_t errcode, ret;

	for (sub_dir = 0; sub_dir < NUMSUBDIR; sub_dir++) {
		snprintf(tempname, METAPATHLEN, "%s/sub_%d",
		         RESTORE_METAPATH, sub_dir);

		/* Creates meta path for meta subfolder if it does not exist
		in restoration folders */
		if (access(tempname, F_OK) == -1)
			MKDIR(tempname, 0700);
	}

	for (sub_dir = 0; sub_dir < NUMSUBDIR; sub_dir++) {
		snprintf(tempname, METAPATHLEN, "%s/sub_%d",
		         RESTORE_BLOCKPATH, sub_dir);

		/* Creates block path for block subfolder
		if it does not exist in restoration folders */
		if (access(tempname, F_OK) == -1)
			MKDIR(tempname, 0700);
	}

	return 0;

errcode_handle:
	return errcode;
}

void* _download_minimal_worker(void *ptr)
{
	int32_t count;
/* FEATURE TODO: Enhanced design for efficient download */
/* Now only create a workable version (single thread perhaps?) */
	UNUSED(ptr);

	if (CURRENT_BACKEND != NONE) {
		sem_init(&download_curl_sem, 0, MAX_DOWNLOAD_CURL_HANDLE);
		sem_init(&download_curl_control_sem, 0, 1);
		sem_init(&nonread_download_curl_sem, 0, MAX_PIN_DL_CONCURRENCY);
		for (count = 0; count < MAX_DOWNLOAD_CURL_HANDLE; count++) {
			snprintf(download_curl_handles[count].id,
				 sizeof(((CURL_HANDLE *)0)->id) - 1,
				"download_thread_%d", count);
			curl_handle_mask[count] = FALSE;
			download_curl_handles[count].curl_backend = NONE;
			download_curl_handles[count].curl = NULL;
		}
	} else {
		notify_restoration_result(1, -EINVAL);
		return NULL;
	}

	run_download_minimal();
	return NULL;
}

void start_download_minimal(void)
{
	int32_t ret;

	ret = _check_and_create_restorepaths();

	if (ret < 0) {
		notify_restoration_result(1, ret);
		return;
	}

	pthread_attr_init(&(download_minimal_attr));
	pthread_attr_setdetachstate(&(download_minimal_attr),
			PTHREAD_CREATE_DETACHED);
	pthread_create(&(download_minimal_thread),
			&(download_minimal_attr),
			_download_minimal_worker, NULL);
	write_log(10, "Forked download minimal threads\n");
}

int32_t fetch_restore_meta_path(char *pathname, ino_t this_inode)
{
	char tempname[METAPATHLEN];
	int32_t sub_dir;

	sub_dir = this_inode % NUMSUBDIR;
	snprintf(tempname, METAPATHLEN, "%s/sub_%d", RESTORE_METAPATH, sub_dir);

	snprintf(pathname, METAPATHLEN, "%s/sub_%d/meta%" PRIu64 "",
		RESTORE_METAPATH, sub_dir, (uint64_t)this_inode);

	return 0;
}

int32_t fetch_restore_block_path(char *pathname, ino_t this_inode, int64_t block_num)
{
	char tempname[BLOCKPATHLEN];
	int32_t sub_dir;

	sub_dir = (this_inode + block_num) % NUMSUBDIR;
	snprintf(tempname, BLOCKPATHLEN, "%s/sub_%d", RESTORE_BLOCKPATH, sub_dir);

	snprintf(pathname, BLOCKPATHLEN, "%s/sub_%d/block%" PRIu64 "_%"PRId64,
			RESTORE_BLOCKPATH, sub_dir, (uint64_t)this_inode, block_num);

	return 0;
}

/* FEATURE TODO: How to retry stage 1 without downloading the same
files again, but also need to ensure the correctness of downloaded files */
int32_t restore_fetch_obj(char *objname, char *despath, BOOL is_meta)
{
	FILE *fptr;
	int32_t ret;

	fptr = fopen(despath, "w+");
	if (fptr == NULL) {
		write_log(0, "Unable to open file for writing\n");
		return -EIO;
	}
	setbuf(fptr, NULL);

	ret = fetch_object_busywait_conn(fptr, RESTORE_FETCH_OBJ, objname);
	if (ret < 0) {
		if (ret == -ENOENT) {
			write_log(0,
				"Critical object not found in restoration\n");
			write_log(0, "Missing obj name: %s\n", objname);
		}
		fclose(fptr);
		unlink(despath);
		return ret;
	}

	if (is_meta == TRUE) {
		/* Reset block status to ST_CLOUD */
		ret = restore_meta_structure(fptr);
	}

	fclose(fptr);
	return ret;
}

int32_t _fetch_meta(ino_t thisinode)
{
	char objname[METAPATHLEN];
	char despath[METAPATHLEN];
	int32_t ret;

	snprintf(objname, sizeof(objname), "meta_%" PRIu64 "",
		 (uint64_t)thisinode);
	fetch_restore_meta_path(despath, thisinode);

	ret = restore_fetch_obj(objname, despath, TRUE);
	return ret;
}

int32_t _fetch_block(ino_t thisinode, int64_t blockno, int64_t seq)
{
	char objname[BLOCKPATHLEN];
	char despath[BLOCKPATHLEN];
	int32_t ret;

	sprintf(objname, "data_%"PRIu64"_%"PRId64"_%"PRIu64,
		(uint64_t)thisinode, blockno, seq);
	fetch_restore_block_path(despath, thisinode, blockno);

	ret = restore_fetch_obj(objname, despath, FALSE);
	return ret;
}

int32_t _fetch_FSstat(ino_t rootinode)
{
	char objname[METAPATHLEN];
	char despath[METAPATHLEN];
	int32_t ret, errcode;

	snprintf(objname, METAPATHLEN - 1, "FSstat%" PRIu64 "",
		 (uint64_t)rootinode);
	snprintf(despath, METAPATHLEN - 1, "%s/FS_sync",
	         RESTORE_METAPATH);
	if (access(despath, F_OK) < 0)
		MKDIR(despath, 0700);
	snprintf(despath, METAPATHLEN - 1, "%s/FS_sync/FSstat%" PRIu64 "",
		 RESTORE_METAPATH, (uint64_t)rootinode);

	ret = restore_fetch_obj(objname, despath, FALSE);
	return ret;

errcode_handle:
	return errcode;
}

int32_t _update_FS_stat(ino_t rootinode)
{
	char despath[METAPATHLEN];
	int32_t errcode;
	FILE *fptr;
	FS_CLOUD_STAT_T tmpFSstat;
	size_t ret_size;
	int64_t after_add_pinsize, delta_pin_size;
	int64_t restored_meta_limit, after_add_metasize, delta_meta_size;
	SYSTEM_DATA_TYPE *restored_system_meta, *rectified_system_meta;

	snprintf(despath, METAPATHLEN - 1, "%s/FS_sync",
	         RESTORE_METAPATH);
	snprintf(despath, METAPATHLEN - 1, "%s/FS_sync/FSstat%" PRIu64 "",
		 RESTORE_METAPATH, (uint64_t)rootinode);

	fptr = fopen(despath, "r");
	if (fptr == NULL) {
		errcode = -errno;
		write_log(0, "Unable to open FS stat for restoration (%d)\n",
		          -errcode);
		return errcode;
	}
	FREAD(&tmpFSstat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
	fclose(fptr);

	LOCK_RESTORED_SYSMETA();
	restored_system_meta =
			&(hcfs_restored_system_meta->restored_system_meta);
	/* Estimate pre-allocated pinned size */
	after_add_pinsize = restored_system_meta->pinned_size +
		(tmpFSstat.pinned_size + 4096 * tmpFSstat.backend_num_inodes);
	if (after_add_pinsize > MAX_PINNED_LIMIT)
		delta_pin_size =
			MAX_PINNED_LIMIT - restored_system_meta->pinned_size;
	else
		delta_pin_size =
			after_add_pinsize - restored_system_meta->pinned_size;
	/* Estimate pre-allocated meta size. */
	restored_meta_limit = META_SPACE_LIMIT - RESERVED_META_MARGIN;
	after_add_metasize = restored_system_meta->system_meta_size +
		(tmpFSstat.backend_meta_size + 4096 * tmpFSstat.backend_num_inodes);

	if (after_add_metasize > restored_meta_limit)
		delta_meta_size =
			restored_meta_limit - restored_system_meta->system_meta_size;
	else
		delta_meta_size =
			after_add_metasize - restored_system_meta->system_meta_size;

	/* Restored system space usage. it will be rectified after
	 * restoration completed */
	restored_system_meta->system_size += tmpFSstat.backend_system_size;
	restored_system_meta->system_meta_size += delta_meta_size;
	restored_system_meta->pinned_size += delta_pin_size; /* Estimated pinned size */
	restored_system_meta->backend_size += tmpFSstat.backend_system_size;
	restored_system_meta->backend_meta_size += tmpFSstat.backend_meta_size;
	restored_system_meta->backend_inodes += tmpFSstat.backend_num_inodes;

	/* rectified space usage */
	rectified_system_meta =
			&(hcfs_restored_system_meta->rectified_system_meta);
	rectified_system_meta->system_size += tmpFSstat.backend_system_size;
	rectified_system_meta->system_meta_size += delta_meta_size;
	rectified_system_meta->pinned_size += delta_pin_size; /* Estimated pinned size */
	rectified_system_meta->backend_size += tmpFSstat.backend_system_size;
	rectified_system_meta->backend_meta_size += tmpFSstat.backend_meta_size;
	rectified_system_meta->backend_inodes += tmpFSstat.backend_num_inodes;
	UNLOCK_RESTORED_SYSMETA();

	return 0;

errcode_handle:
	fclose(fptr);
	return errcode;
}

int32_t _fetch_pinned(ino_t thisinode)
{
	FILE_META_TYPE tmpmeta;
	HCFS_STAT tmpstat;
	FILE *fptr;
	char metapath[METAPATHLEN];
	int64_t count, totalblocks, tmpsize, seq;
	int64_t nowpage, lastpage, filepos, nowindex;
	int64_t num_cached_block = 0, cached_size = 0;
	int32_t errcode, ret;
	size_t ret_size;
	struct stat blockstat;
	BLOCK_ENTRY_PAGE temppage;
	FILE_STATS_TYPE file_stats_type;
	BOOL write_page;
	char blockpath[BLOCKPATHLEN];
	int64_t file_stats_pos;

	fetch_restore_meta_path(metapath, thisinode);
	fptr = fopen(metapath, "r+");
	if (fptr == NULL) {
		write_log(0, "Error when fetching file to restore\n");
		errcode = -errno;
		return errcode;
	}
	setbuf(fptr, NULL);

	FREAD(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
	FREAD(&tmpmeta, sizeof(FILE_META_TYPE), 1, fptr);
	if (P_IS_UNPIN(tmpmeta.local_pin)) {
		/* Don't fetch blocks */
		fclose(fptr);
		return 0;
	}

	/* Assuming fixed block size now */
	write_page = FALSE;
	tmpsize = tmpstat.size;
	totalblocks = ((tmpsize - 1) / MAX_BLOCK_SIZE) + 1;
	lastpage = -1;
	for (count = 0; count < totalblocks; count++) {
		nowpage = count / MAX_BLOCK_ENTRIES_PER_PAGE;
		nowindex = count % MAX_BLOCK_ENTRIES_PER_PAGE;
		if (lastpage != nowpage) {
			if (write_page == TRUE) {
				FSEEK(fptr, filepos, SEEK_SET);
				FWRITE(&temppage, sizeof(BLOCK_ENTRY_PAGE),
						1, fptr);
				write_page = FALSE;
			}
			/* Reload page pos */
			filepos = seek_page2(&tmpmeta, fptr, nowpage, 0);
			if (filepos < 0) {
				errcode = (int32_t) filepos;
				goto errcode_handle;
			}
			if (filepos == 0) {
				/* No page to be found */
				count += (BLK_INCREMENTS - 1);
				continue;
			}
			write_log(10, "Debug fetch: %" PRId64 ", %" PRId64 "\n",
			          filepos, nowpage);
			FSEEK(fptr, filepos, SEEK_SET);
			memset(&temppage, 0, sizeof(BLOCK_ENTRY_PAGE));
			FREAD(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
			lastpage = nowpage;
		}

		/* Terminate pin blocks downloading if system is going down */
		if (hcfs_system->system_going_down == TRUE) {
			errcode = -ESHUTDOWN;
			goto errcode_handle;
		}

		/* Skip if block does not exist */
		if (temppage.block_entries[nowindex].status == ST_CLOUD) {
			seq = temppage.block_entries[nowindex].seqnum;
			ret = _fetch_block(thisinode, count, seq);
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}
			/* Change block status in meta */
			temppage.block_entries[nowindex].status = ST_BOTH;
			write_page = TRUE;
			/* Update file stats */
			fetch_restore_block_path(blockpath, thisinode, count);
			ret = stat(blockpath, &blockstat);
			if (ret == 0) {
				cached_size += blockstat.st_blocks * 512;
				num_cached_block += 1;
			} else {
				write_log(0, "Error: Fail to stat block in %s."
					" Code %d", __func__, errno);
			}
		}
	}
	if (write_page == TRUE) {
		FSEEK(fptr, filepos, SEEK_SET);
		FWRITE(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
		write_page = FALSE;
	}
	/* Update file stats */
	file_stats_pos = offsetof(FILE_META_HEADER, fst);
	FSEEK(fptr, file_stats_pos, SEEK_SET);
	FREAD(&file_stats_type, sizeof(FILE_STATS_TYPE), 1, fptr);
	file_stats_type.num_cached_blocks = num_cached_block;
	file_stats_type.cached_size = cached_size;
	FSEEK(fptr, file_stats_pos, SEEK_SET);
	FWRITE(&file_stats_type, sizeof(FILE_STATS_TYPE), 1, fptr);
	fclose(fptr);

	/* Update cache statistics */
	update_restored_cache_usage(cached_size, num_cached_block);

	return 0;
errcode_handle:
	fclose(fptr);
	write_log(0, "Restore Error: Code %d\n", -errcode);
	write_log(10, "Error in %s.\n", __func__);
	return errcode;
}

int32_t _check_expand(ino_t thisinode, char *nowpath, int32_t depth)
{
	UNUSED(thisinode);

	if (strcmp(nowpath, "/data/app") == 0)
		return 1;

	if (strcmp(nowpath, "/data/data") == 0)
		return 1;

	/* Expand /storage/emulated/Android */
	/* Expand /storage/emulated/<x>/Android, where x is a natural number */
	if (strcmp(nowpath, "/storage/emulated") == 0)
		return 2;

	/* If not high-priority pin, in /data/data only keep the lib symlinks */
	if ((strncmp(nowpath, "/data/data", strlen("/data/data")) == 0) &&
	    (depth == 1))
		return 3;

	if ((strncmp(nowpath, "/storage/emulated/Android",
	     strlen("/storage/emulated/Android")) == 0) &&
	    ((depth == 1) || (depth == 2)))
		return 1;

	/* If this is /storage/emulated/<x>/Android */
	if ((strncmp(nowpath, "/storage/emulated",
	     strlen("/storage/emulated")) == 0) &&
	    ((depth == 2) || (depth == 3)))
		return 1;

	/* If this is /storage/emulated/<x> */
	if ((strncmp(nowpath, "/storage/emulated",
	     strlen("/storage/emulated")) == 0) &&
	    (depth == 1))
		return 4;

	return 0;
}
int32_t _expand_and_fetch(ino_t thisinode, char *nowpath, int32_t depth)
{
	FILE *fptr;
	char fetchedmeta[METAPATHLEN];
	char tmppath[METAPATHLEN];
	DIR_META_TYPE dirmeta;
	DIR_ENTRY_PAGE tmppage;
	int64_t filepos;
	int32_t count;
	ino_t tmpino;
	DIR_ENTRY *tmpptr;
	int32_t expand_val;
	BOOL skip_this;
	int32_t ret, errcode;
	size_t ret_size;

	fetch_restore_meta_path(fetchedmeta, thisinode);
	fptr = fopen(fetchedmeta, "r");
	if (fptr == NULL) {
		write_log(0, "Error when fetching file to restore\n");
		errcode = -errno;
		return errcode;
	}

	FSEEK(fptr, sizeof(HCFS_STAT), SEEK_SET);
	FREAD(&dirmeta, sizeof(DIR_META_TYPE), 1, fptr);

	/* Do not expand if not high priority pin and not needed */
	expand_val = 1;  /* The default */
	if (dirmeta.local_pin != P_HIGH_PRI_PIN) {
		expand_val = _check_expand(thisinode, nowpath, depth);
		if (expand_val == 0)
			return 0;
	}

	/* Fetch first page */
	filepos = dirmeta.tree_walk_list_head;

	while (filepos != 0) {
		if (hcfs_system->system_going_down == TRUE) {
			errcode = -ESHUTDOWN;
			goto errcode_handle;
		}
		FSEEK(fptr, filepos, SEEK_SET);
		FREAD(&tmppage, sizeof(DIR_ENTRY_PAGE), 1, fptr);
		write_log(10, "Filepos %lld, entries %d\n", filepos,
		       tmppage.num_entries);
		for (count = 0; count < tmppage.num_entries; count++) {
			tmpptr = &(tmppage.dir_entries[count]);
			
			if (tmpptr->d_ino == 0)
				continue;
			/* Skip "." and ".." */
			if (strcmp(tmpptr->d_name, ".") == 0)
				continue;
			if (strcmp(tmpptr->d_name, "..") == 0)
				continue;

			skip_this = FALSE;
			switch (expand_val) {
			case 2:
				if (strcmp(tmpptr->d_name, "Android") == 0)
					break;
				if (is_natural_number(tmpptr->d_name) == TRUE)
					break;
				skip_this = TRUE;
				break;
			case 4:
				if (strcmp(tmpptr->d_name, "Android") != 0)
					skip_this = TRUE;
				break;
			case 3:
				if (strcmp(tmpptr->d_name, "lib") != 0)
					skip_this = TRUE;
				break;
			default:
				break;
			}

			if (skip_this == TRUE)
				continue;

			write_log(10, "Processing %s/%s\n", nowpath,
			          tmpptr->d_name);

			/* FEATURE TODO: For high-priority pin dirs in
			/data/app and in emulated, if missing, could
			just prune the app out (will need to verify
			though). Check the HL design */
			/* FEATURE TODO: Will need to handle ENOENT
			errors here. Some can be fixed and the restoration
			can continue (such as broken user apps), others
			not */
			/* First fetch the meta */
			tmpino = tmpptr->d_ino;
			ret = _fetch_meta(tmpino);
			if (ret < 0) {
				/* FEATURE TODO: error handling,
				such as shutdown */
				errcode = ret;
				goto errcode_handle;
			}

			switch (tmpptr->d_type) {
			case D_ISLNK:
				/* Just fetch the meta */
				break;
			case D_ISREG:
			case D_ISFIFO:
			case D_ISSOCK:
				/* Fetch all blocks if pinned */
				ret = _fetch_pinned(tmpino);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}
				break;
			case D_ISDIR:
				/* Need to expand */
				snprintf(tmppath, METAPATHLEN, "%s/%s",
				         nowpath, tmpptr->d_name);
				ret = _expand_and_fetch(tmpino, tmppath,
				                        depth + 1);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}
				break;
			default:
				break;
			}
		}
		/* Continue to the next page */
		filepos = tmppage.tree_walk_next;
	}
	fclose(fptr);
	return 0;
errcode_handle:
	fclose(fptr);
	return errcode;
}

/* This is a helper function for writing reconstructed system stat to
hcfssystemfile in restored meta storage folder */
int32_t _rebuild_system_meta(void)
{
	char restored_sysmeta[METAPATHLEN];
	FILE *fptr;
	int32_t ret, errcode;
	size_t ret_size;

	LOCK_RESTORED_SYSMETA();
	snprintf(restored_sysmeta, METAPATHLEN, "%s/hcfssystemfile",
	         RESTORE_METAPATH);
	fptr = fopen(restored_sysmeta, "w");
	if (fptr == NULL) {
		errcode = -errno;
		write_log(0, "Unable to open sys file for restoration (%d)\n",
		          -errcode);
		return errcode;
	}
	FWRITE(&hcfs_restored_system_meta->restored_system_meta,
			sizeof(SYSTEM_DATA_TYPE), 1, fptr);
	fclose(fptr);

	/* Backup rectified space usage, which is used to re-compute
	 * system usage in restoration stage 2. */
	fptr = hcfs_restored_system_meta->rect_fptr;
	FSEEK(fptr, 0, SEEK_SET);
	FWRITE(&hcfs_restored_system_meta->rectified_system_meta,
			sizeof(SYSTEM_DATA_TYPE), 1, fptr);
	fclose(fptr);
	UNLOCK_RESTORED_SYSMETA();

	return 0;

errcode_handle:
	fclose(fptr);
	UNLOCK_RESTORED_SYSMETA();
	return errcode;
}

int32_t _restore_system_quota(void)
{
	char srcpath[METAPATHLEN];
	char despath[METAPATHLEN];
	int32_t ret, errcode;


        sem_wait(&(download_usermeta_ctl.access_sem));
        if (download_usermeta_ctl.active == TRUE) {
                sem_post(&(download_usermeta_ctl.access_sem));
                write_log(0, "Quota download is already in progress?\n");
                return -EBUSY;
        } else {
                download_usermeta_ctl.active = TRUE;
        }
	sem_post(&(download_usermeta_ctl.access_sem));

	fetch_quota_from_cloud(NULL, FALSE);

	/* Need to rename quota backup from metastorage to metastore_restore */
	snprintf(srcpath, sizeof(srcpath), "%s/usermeta", METAPATH);
	snprintf(despath, sizeof(despath), "%s/usermeta", RESTORE_METAPATH);

	ret = rename(srcpath, despath);
	if (ret < 0) {
		errcode = -errno;
		write_log(0, "Unable to fetch quota in restoration (%d)\n",
		          -errcode);
		return errcode;
	}

	return 0;
}

void _init_quota_restore()
{
	/* Init usermeta curl handle */
	snprintf(download_usermeta_curl_handle.id,
	         sizeof(((CURL_HANDLE *)0)->id) - 1, "download_usermeta");
	download_usermeta_curl_handle.curl_backend = NONE;
	download_usermeta_curl_handle.curl = NULL;

	/* Setup download control */
	memset(&download_usermeta_ctl, 0, sizeof(DOWNLOAD_USERMETA_CTL));
	sem_init(&(download_usermeta_ctl.access_sem), 0, 1);
}

/* FEATURE TODO: Need a notify and retry mechanism if network is down */
int32_t run_download_minimal(void)
{
	ino_t rootino;
	char despath[METAPATHLEN];
	DIR_META_TYPE tmp_head;
	DIR_ENTRY_PAGE tmppage;
	FILE *fptr;
	int32_t errcode, count, ret;
	ssize_t ret_ssize;
	DIR_ENTRY *tmpentry;
	BOOL is_fopen = FALSE;

	/* Fetch quota value from backend and store in the restoration path */

	/* First make sure that network connection is turned on */
	while (hcfs_system->sync_paused == TRUE) {
		write_log(4, "Connection is not available now\n");
		write_log(4, "Sleep for 5 seconds before retrying\n");
		notify_restoration_result(1, -ENETDOWN);
		sleep(5);
		if (hcfs_system->system_going_down == TRUE)
			return -ESHUTDOWN;
	}

	_init_quota_restore();
	ret = _restore_system_quota();
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	write_log(4, "Downloading package list backup\n");
	snprintf(despath, METAPATHLEN, "%s/backup_pkg", RESTORE_METAPATH);
	ret = restore_fetch_obj("backup_pkg", despath, FALSE);

	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	/* Init rectified system meta statistics */
	ret = init_rectified_system_meta(RESTORING_STAGE1);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	snprintf(despath, METAPATHLEN, "%s/fsmgr", RESTORE_METAPATH);
	ret = restore_fetch_obj("FSmgr_backup", despath, FALSE);

	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	/* Parse file mgr */
	fptr = fopen(despath, "r");
	if (fptr == NULL) {
		write_log(0, "Error when parsing volumes to restore\n");
		errcode = -errno;
		return errcode;
	}
	is_fopen = TRUE;

	setbuf(fptr, NULL);

	PREAD(fileno(fptr), &tmp_head, sizeof(DIR_META_TYPE),
	      16);
	PREAD(fileno(fptr), &tmppage, sizeof(DIR_ENTRY_PAGE),
	      tmp_head.tree_walk_list_head);
	fclose(fptr);
	is_fopen = FALSE;

	for (count = 0; count < tmppage.num_entries; count++) {
		tmpentry = &(tmppage.dir_entries[count]);
		write_log(4, "Processing minimal for %s\n",
		          tmpentry->d_name);
		if (!strcmp("hcfs_app", tmpentry->d_name)) {
			rootino = tmpentry->d_ino;
			ret = _fetch_meta(rootino);
			if (ret == 0)
				ret = _fetch_FSstat(rootino);
			if (ret == 0)
				ret = _update_FS_stat(rootino);
			if (ret == 0)
				ret = _expand_and_fetch(rootino,
						"/data/app", 0);
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}
			continue;
		}
		if (!strcmp("hcfs_data", tmpentry->d_name)) {
			rootino = tmpentry->d_ino;
			ret = _fetch_meta(rootino);
			if (ret == 0)
				ret = _fetch_FSstat(rootino);
			if (ret == 0)
				ret = _update_FS_stat(rootino);
			if (ret == 0)
				ret = _expand_and_fetch(rootino,
						"/data/data", 0);
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}
			continue;
		}
		if (!strcmp("hcfs_external", tmpentry->d_name)) {
			rootino = tmpentry->d_ino;
			ret = _fetch_meta(rootino);
			if (ret == 0)
				ret = _fetch_FSstat(rootino);
			if (ret == 0)
				ret = _update_FS_stat(rootino);
			if (ret == 0)
				ret = _expand_and_fetch(rootino,
						"/storage/emulated", 0);
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}
			continue;
		}
	}

	/* Rebuild hcfssystemmeta in the restoration path */
	ret = _rebuild_system_meta();
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	sem_wait(&restore_sem);

	/* Tag status of restoration */
	ret = tag_restoration("rebuilding_meta");
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}
	sync();

	sem_post(&restore_sem);

	/* FEATURE TODO: Move the renaming to the start of stage 2 */
	/* Renaming package list backup to the original location */
	/* FEATURE TODO: If need to make sure that package list is backed up,
	perhaps should delay upload sync to after package data creation has
	stopped for a few seconds */
	snprintf(despath, METAPATHLEN, "%s/backup_pkg", RESTORE_METAPATH);
	rename(despath, PACKAGE_XML);
	chown(PACKAGE_XML, SYSTEM_UID, SYSTEM_GID);
	chmod(PACKAGE_XML, 0660);
	system("restorecon /data/system/packages.xml");
	
	unlink(PACKAGE_LIST);  /* Need to regenerate packages.list */

	notify_restoration_result(1, 0);

	return 0;
errcode_handle:
	if (is_fopen == TRUE)
		fclose(fptr);
	notify_restoration_result(1, errcode);
	return errcode;
}

int _delete_node(const char *thispath, const struct stat *thisstat,
		int flag, struct FTW *buf)
{
	int ret, errcode;

	UNUSED(buf);
	UNUSED(thisstat);
	errcode = 0;
	switch (flag) {
	case FTW_F:
		UNLINK(thispath);
		break;
	case FTW_D:
		write_log(4, "Unprocessed files in deleting unused content?\n");
		errcode = -EAGAIN;
		break;
	case FTW_DP:
		RMDIR(thispath);
		break;
	default:
		write_log(4, "Unexpected error in deleting unused content?\n");
		errcode = -EIO;
		break;
	}
	return errcode;

errcode_handle:
	write_log(4, "IO error causing deleting unused content to terminate\n");
	return errcode;
}

void cleanup_stage1_data(void)
{
	char todelete_metapath[METAPATHLEN];
	char todelete_blockpath[BLOCKPATHLEN];

	snprintf(todelete_metapath, METAPATHLEN, "%s_todelete",
	         METAPATH);
	snprintf(todelete_blockpath, BLOCKPATHLEN, "%s_todelete",
	         BLOCKPATH);

	if (access(todelete_metapath, F_OK) == 0)
		nftw(todelete_metapath, _delete_node, 10,
		     FTW_DEPTH | FTW_PHYS | FTW_MOUNT);

	if (access(todelete_blockpath, F_OK) == 0)
		nftw(todelete_blockpath, _delete_node, 10,
		     FTW_DEPTH | FTW_PHYS | FTW_MOUNT);
}

/* Function for backing up package list (packages.xml) */
int32_t backup_package_list(void)
{
	sem_wait(&backup_pkg_sem);
	have_new_pkgbackup = TRUE;
	sem_post(&backup_pkg_sem);
	return 0;
}

/**
 * Update rectified system meta when restoring.
 *
 * @param delta_system_meta Structure of delta system space usage.
 *
 * @return none.
 */
void update_rectified_system_meta(DELTA_SYSTEM_META delta_system_meta)
{
	SYSTEM_DATA_TYPE *rectified_system_meta;
	int64_t ret_ssize;
	int32_t errcode;

	rectified_system_meta =
			&(hcfs_restored_system_meta->rectified_system_meta);

	LOCK_RESTORED_SYSMETA();
	/* Update rectified space usage */
	rectified_system_meta->system_size +=
			delta_system_meta.delta_system_size;
	rectified_system_meta->system_meta_size +=
			delta_system_meta.delta_meta_size;
	rectified_system_meta->pinned_size +=
			delta_system_meta.delta_pinned_size;
	rectified_system_meta->backend_size +=
			delta_system_meta.delta_backend_size;
	rectified_system_meta->backend_meta_size +=
			delta_system_meta.delta_backend_meta_size;
	rectified_system_meta->backend_inodes +=
			delta_system_meta.delta_backend_inodes;
	if (hcfs_restored_system_meta->rect_fptr)
		PWRITE(fileno(hcfs_restored_system_meta->rect_fptr),
			rectified_system_meta, sizeof(SYSTEM_DATA_TYPE), 0);
	UNLOCK_RESTORED_SYSMETA();
	return;

errcode_handle:
	return;
}

/**
 * Update cache usage in restoration stage 1. Do NOT call this function
 * in stage 2 because cache usage in stage 2 is controlled by pinning
 * scheduler, such as downloading blocks for a PIN file.
 *
 * @param delta_cache_size Delta cache size, which is block unit size.
 * @param delta_cache_blocks Change of local block number.
 *
 * @return none.
 */
void update_restored_cache_usage(int64_t delta_cache_size,
		int64_t delta_cache_blocks)
{
	SYSTEM_DATA_TYPE *restored_system_meta;

	restored_system_meta =
			&(hcfs_restored_system_meta->restored_system_meta);

	LOCK_RESTORED_SYSMETA();
	restored_system_meta->cache_size += delta_cache_size;
	if (restored_system_meta->cache_size < 0)
		restored_system_meta->cache_size = 0;

	restored_system_meta->cache_blocks += delta_cache_blocks;
	if (restored_system_meta->cache_blocks < 0)
		restored_system_meta->cache_blocks = 0;
	UNLOCK_RESTORED_SYSMETA();

	return;
}

/**
 * Rectify the space usage in last step of restoration stage 2.
 *
 * @return 0 on success, otherwise negative errcode.
 */
int32_t rectify_space_usage()
{
	SYSTEM_DATA_TYPE *rectified_system_meta;
	int32_t ret, errcode;
	char rectified_usage_path[METAPATHLEN];

	rectified_system_meta =
			&(hcfs_restored_system_meta->rectified_system_meta);

	write_log(4, "Info: rectified_system_size = %lld",
			rectified_system_meta->system_size);
	write_log(4, "Info: rectified_system_meta_size = %lld",
			rectified_system_meta->system_meta_size);
	write_log(4, "Info: rectified_backend_size = %lld",
			rectified_system_meta->backend_size);
	write_log(4, "Info: rectified_beckend_meta_size = %lld",
			rectified_system_meta->backend_meta_size);
	write_log(4, "Info: rectified_backend_inodes = %lld",
			rectified_system_meta->backend_inodes);
	write_log(4, "Info: rectified_pinned_size = %lld",
			rectified_system_meta->pinned_size);

	/* Rectify the statistics. When restoration is complete, decrease
	 * hcfs space usage statistics by error value recorded in
	 * rectified_system_meta */
	LOCK_RESTORED_SYSMETA();
	change_system_meta(
		-rectified_system_meta->system_size,
		-rectified_system_meta->system_meta_size,
		0, 0, 0, 0, TRUE);
	update_backend_usage(
		-rectified_system_meta->backend_size,
		-rectified_system_meta->backend_meta_size,
		-rectified_system_meta->backend_inodes);
	change_pin_size(-rectified_system_meta->pinned_size);
	UNLOCK_RESTORED_SYSMETA();

	/* Remove the file */
	snprintf(rectified_usage_path, METAPATHLEN, "%s/hcfssystemfile.rectified",
	         METAPATH);
	if (hcfs_restored_system_meta->rect_fptr)
		fclose(hcfs_restored_system_meta->rect_fptr);
	if (!access(rectified_usage_path, F_OK))
		UNLINK(rectified_usage_path);
	sem_destroy(&(hcfs_restored_system_meta->sysmeta_sem));
	FREE(hcfs_restored_system_meta);
	return 0;

errcode_handle:
	FREE(hcfs_restored_system_meta);
	return errcode;
}

/**
 * Init the system meta used to rectify space usage. Create the rectified
 * system meta file if it does not exist. Otherwise open it and load to memory.
 *
 * @param restoration_stage Restoration stage tag used to find the rectified
 *                          system meta file.
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t init_rectified_system_meta(char restoration_stage)
{
	char rectified_usage_path[METAPATHLEN];
	int32_t errcode;
	int64_t ret_ssize;
	BOOL open = FALSE;

	hcfs_restored_system_meta = (HCFS_RESTORED_SYSTEM_META *)
			malloc(sizeof(HCFS_RESTORED_SYSTEM_META));
	memset(hcfs_restored_system_meta, 0,
			sizeof(HCFS_RESTORED_SYSTEM_META));
	sem_init(&(hcfs_restored_system_meta->sysmeta_sem), 0, 1);

	/* Check the path */
	if (restoration_stage == RESTORING_STAGE1) /* Stage 1 */
		snprintf(rectified_usage_path, METAPATHLEN,
			"%s/hcfssystemfile.rectified", RESTORE_METAPATH);
	else if (restoration_stage == RESTORING_STAGE2) /* Stage 2 */
		snprintf(rectified_usage_path, METAPATHLEN,
			"%s/hcfssystemfile.rectified", METAPATH);
	else
		return -EINVAL;

	if (hcfs_system->system_restoring == RESTORING_STAGE2 &&
			access(rectified_usage_path, F_OK) == 0) {
		/* Open the rectified system meta and load it. */
		hcfs_restored_system_meta->rect_fptr =
				fopen(rectified_usage_path, "r+");
		if (hcfs_restored_system_meta->rect_fptr == NULL) {
			errcode = errno;
			write_log(0, "Error: Fail to open file in %s. Code %d",
				__func__, errcode);
			goto errcode_handle;
		}
		open = TRUE;
		setbuf(hcfs_restored_system_meta->rect_fptr, NULL);
		PREAD(fileno(hcfs_restored_system_meta->rect_fptr),
			&(hcfs_restored_system_meta->rectified_system_meta),
			sizeof(SYSTEM_DATA_TYPE), 0);

	} else {
		/* In restoration stage 1, always create a new rectified file
		 * and re-estimate the system meta. */
		if (hcfs_system->system_restoring == RESTORING_STAGE1)
			write_log(8, "Create a rectified system meta file.");
		else
			write_log(2, "Warn: Rectified system meta file"
				" not found in stage 2. Create an empty one.");
		/* Create a new one */
		hcfs_restored_system_meta->rect_fptr =
				fopen(rectified_usage_path, "w+");
		if (hcfs_restored_system_meta->rect_fptr == NULL) {
			errcode = errno;
			write_log(0, "Error: Fail to open file in %s. Code %d",
					__func__, errcode);
			goto errcode_handle;
		}
		open = TRUE;
		setbuf(hcfs_restored_system_meta->rect_fptr, NULL);
		/* Write zero */
		PWRITE(fileno(hcfs_restored_system_meta->rect_fptr),
			&(hcfs_restored_system_meta->rectified_system_meta),
			sizeof(SYSTEM_DATA_TYPE), 0);
	}

	return 0;

errcode_handle:
	if (open)
		fclose(hcfs_restored_system_meta->rect_fptr);
	hcfs_restored_system_meta->rect_fptr = NULL;
	return errcode;
}
