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

#include <errno.h>
#include <ftw.h>
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "dir_entry_btree.h"
#include "event_filter.h"
#include "event_notification.h"
#include "fuseop.h"
#include "global.h"
#include "hcfs_fromcloud.h"
#include "macro.h"
#include "metaops.h"
#include "mount_manager.h"
#include "utils.h"
#include "restoration_utils.h"

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE

void init_restore_path(void)
{
	snprintf(RESTORE_METAPATH, METAPATHLEN, "%s_restore", METAPATH);
	snprintf(RESTORE_BLOCKPATH, BLOCKPATHLEN, "%s_restore", BLOCKPATH);
	sem_init(&(restore_sem), 0, 1);
	sem_init(&(backup_pkg_sem), 0, 1);
	have_new_pkgbackup = TRUE;
	use_old_cloud_stat = FALSE;
}

int32_t fetch_restore_stat_path(char *pathname)
{
	snprintf(pathname, METAPATHLEN, "%s/system_restoring_status", METAPATH);
	return 0;
}

/************************************************************************
*
* Function name: fetch_restore_todelete_path
*        Inputs: char *pathname, ino_t this_inode
*        Output: Integer
*       Summary: Given the inode number this_inode,
*                copy the filename to the meta file in todelete folder
*                to the space pointed by pathname for restoration stage 1.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t fetch_restore_todelete_path(char *pathname, ino_t this_inode)
{
	char tempname[METAPATHLEN];
	int32_t sub_dir;
	int32_t errcode, ret;

	if (RESTORE_METAPATH == NULL)
		return -EPERM;

	sub_dir = this_inode % NUMSUBDIR;
	snprintf(tempname, METAPATHLEN, "%s/todelete", RESTORE_METAPATH);
	if (access(tempname, F_OK) == -1) {
		ret = mkdir(tempname, 0700);
		if (ret < 0) {
			errcode = -errno;
			if (errcode != -EEXIST)
				goto errcode_handle;
		}
	}

	snprintf(tempname, METAPATHLEN, "%s/todelete/sub_%d", RESTORE_METAPATH,
		 sub_dir);
	if (access(tempname, F_OK) == -1) {
		ret = mkdir(tempname, 0700);
		if (ret < 0) {
			errcode = -errno;
			if (errcode != -EEXIST)
				goto errcode_handle;
		}
	}

	snprintf(pathname, METAPATHLEN, "%s/todelete/sub_%d/meta%" PRIu64 "",
		 RESTORE_METAPATH, sub_dir, (uint64_t)this_inode);
	return 0;
errcode_handle:
	return errcode;
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
	if (is_open)
		fclose(fptr);
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
		ret =
		    add_notify_event(RESTORATION_STAGE1_CALLBACK, msgstr, TRUE);
		break;
	case 2:
		/* Restoration stage 2 */
		snprintf(msgstr, 100, "{\"result\":%d}", result);
		ret =
		    add_notify_event(RESTORATION_STAGE2_CALLBACK, msgstr, TRUE);
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
		snprintf(tempname, METAPATHLEN, "%s/sub_%d", RESTORE_METAPATH,
			 sub_dir);

		/* Creates meta path for meta subfolder if it does not exist
		 * in restoration folders
		 */
		if (access(tempname, F_OK) == -1)
			MKDIR(tempname, 0700);
	}

	for (sub_dir = 0; sub_dir < NUMSUBDIR; sub_dir++) {
		snprintf(tempname, METAPATHLEN, "%s/sub_%d", RESTORE_BLOCKPATH,
			 sub_dir);

		/* Creates block path for block subfolder
		 * if it does not exist in restoration folders
		 */
		if (access(tempname, F_OK) == -1)
			MKDIR(tempname, 0700);
	}

	return 0;

errcode_handle:
	return errcode;
}

void *_download_minimal_worker(void *ptr)
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
	pthread_create(&(download_minimal_thread), &(download_minimal_attr),
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

int32_t fetch_restore_block_path(char *pathname,
				 ino_t this_inode,
				 int64_t block_num)
{
	char tempname[BLOCKPATHLEN];
	int32_t sub_dir;

	sub_dir = (this_inode + block_num) % NUMSUBDIR;
	snprintf(tempname, BLOCKPATHLEN, "%s/sub_%d", RESTORE_BLOCKPATH,
		 sub_dir);

	snprintf(pathname, BLOCKPATHLEN, "%s/sub_%d/block%" PRIu64 "_%" PRId64,
		 RESTORE_BLOCKPATH, sub_dir, (uint64_t)this_inode, block_num);

	return 0;
}

/* FEATURE TODO: How to retry stage 1 without downloading the same
 * files again, but also need to ensure the correctness of downloaded files
 */
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

	sprintf(objname, "data_%" PRIu64 "_%" PRId64 "_%" PRIu64,
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
	snprintf(despath, METAPATHLEN - 1, "%s/FS_sync", RESTORE_METAPATH);
	if (access(despath, F_OK) < 0)
		MKDIR(despath, 0700);
	snprintf(despath, METAPATHLEN - 1, "%s/FS_sync/FSstat%" PRIu64 "",
		 RESTORE_METAPATH, (uint64_t)rootinode);

	ret = restore_fetch_obj(objname, despath, FALSE);
	return ret;

errcode_handle:
	return errcode;
}

int32_t _update_FS_stat(ino_t rootinode, ino_t *max_inode)
{
	char despath[METAPATHLEN];
	int32_t errcode;
	FILE *fptr;
	FS_CLOUD_STAT_T tmpFSstat;
	size_t ret_size;
	int64_t after_add_pinsize, delta_pin_size;
	int64_t restored_meta_limit, after_add_metasize, delta_meta_size;
	SYSTEM_DATA_TYPE *restored_system_meta, *rectified_system_meta;

	snprintf(despath, METAPATHLEN - 1, "%s/FS_sync", RESTORE_METAPATH);
	snprintf(despath, METAPATHLEN - 1, "%s/FS_sync/FSstat%" PRIu64 "",
		 RESTORE_METAPATH, (uint64_t)rootinode);

	errcode = convert_cloud_stat_struct(despath);
	if (errcode < 0)
		return errcode;

	fptr = fopen(despath, "r");
	if (fptr == NULL) {
		errcode = -errno;
		write_log(0, "Unable to open FS stat for restoration (%d)\n",
			  -errcode);
		return errcode;
	}
	FREAD(&tmpFSstat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
	fclose(fptr);

	*max_inode = tmpFSstat.max_inode;

	/* FEATURE TODO: Will need to check if cloud stat is converted from
	 * old struct (V1) if can resume download in stage 1
	 */
	if (tmpFSstat.disk_pinned_size < 0)
		use_old_cloud_stat = TRUE;

	LOCK_RESTORED_SYSMETA();
	restored_system_meta =
	    &(hcfs_restored_system_meta->restored_system_meta);
	/* Estimate pre-allocated pinned size if use old cloud stat struct */
	if (tmpFSstat.disk_pinned_size < 0)
		after_add_pinsize = restored_system_meta->pinned_size +
				    (tmpFSstat.pinned_size +
				     4096 * tmpFSstat.backend_num_inodes);
	else
		after_add_pinsize = restored_system_meta->pinned_size +
				    tmpFSstat.disk_pinned_size;

	if (after_add_pinsize > MAX_PINNED_LIMIT)
		delta_pin_size =
		    MAX_PINNED_LIMIT - restored_system_meta->pinned_size;
	else
		delta_pin_size =
		    after_add_pinsize - restored_system_meta->pinned_size;
	/* Estimate pre-allocated meta size if use old cloud stat struct. */
	restored_meta_limit = META_SPACE_LIMIT - RESERVED_META_MARGIN;

	if (tmpFSstat.disk_meta_size < 0)
		after_add_metasize = restored_system_meta->system_meta_size +
				     (tmpFSstat.backend_meta_size +
				      4096 * tmpFSstat.backend_num_inodes);
	else
		after_add_metasize = restored_system_meta->system_meta_size +
				     tmpFSstat.disk_meta_size;

	if (after_add_metasize > restored_meta_limit)
		delta_meta_size = restored_meta_limit -
				  restored_system_meta->system_meta_size;
	else
		delta_meta_size =
		    after_add_metasize - restored_system_meta->system_meta_size;

	/* Restored system space usage. it will be rectified after
	 * restoration completed
	 */
	restored_system_meta->system_size += tmpFSstat.backend_system_size;
	restored_system_meta->system_meta_size += delta_meta_size;
	restored_system_meta->pinned_size +=
	    delta_pin_size; /* Estimated pinned size */
	restored_system_meta->backend_size += tmpFSstat.backend_system_size;
	restored_system_meta->backend_meta_size += tmpFSstat.backend_meta_size;
	restored_system_meta->backend_inodes += tmpFSstat.backend_num_inodes;

	/* rectified space usage */
	rectified_system_meta =
	    &(hcfs_restored_system_meta->rectified_system_meta);
	rectified_system_meta->system_size += tmpFSstat.backend_system_size;
	rectified_system_meta->system_meta_size += delta_meta_size;
	rectified_system_meta->pinned_size +=
	    delta_pin_size; /* Estimated pinned size */
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
	int64_t count = 0, totalblocks, tmpsize, seq, blkcount;
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
				FWRITE(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1,
				       fptr);
				write_page = FALSE;
			}
			/* Reload page pos */
			filepos = seek_page2(&tmpmeta, fptr, nowpage, 0);
			if (filepos < 0) {
				errcode = (int32_t)filepos;
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
				write_log(
				    0,
				    "Error: Fail to stat block in %s. Code %d",
				    __func__, errno);
			}
		}
	}
	if (write_page == TRUE) {
		FSEEK(fptr, filepos, SEEK_SET);
		FWRITE(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
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
	if (errcode == -ENOENT) {
		write_log(4, "Cleaning up blocks of broken file\n");
		for (blkcount = 0; blkcount <= count; blkcount++) {
			fetch_restore_block_path(blockpath, thisinode,
						 blkcount);
			unlink(blockpath);
		}
	}

	write_log(0, "Restore Error: Code %d\n", -errcode);
	write_log(10, "Error in %s.\n", __func__);
	return errcode;
}

int32_t _check_expand(ino_t thisinode, char *nowpath, int32_t depth)
{
	UNUSED(thisinode);

	if (strcmp(nowpath, "/data/app") == 0)
		return 1;

	/* If in /data/app, need to pull down everything now */
	/* App could be installed but not pinned by management app */
	if (strncmp(nowpath, "/data/app", strlen("/data/app")) == 0)
		return 5;

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

/*
 *  Helper function for moving meta files that are deleted to to_delete
 *  folder, and append the inode number to a list so that in stage 2, the
 *  inodes will be entered into the to-delete list
 */
int32_t _mark_delete(ino_t thisinode)
{
	char oldpath[METAPATHLEN];
	char newpath[METAPATHLEN];
	int32_t ret, errcode;
	size_t ret_size;

	ret = fetch_restore_meta_path(oldpath, thisinode);
	if (ret < 0)
		return ret;

	ret = fetch_restore_todelete_path(newpath, thisinode);
	if (ret < 0)
		return ret;

	ret = rename(oldpath, newpath);
	if (ret < 0) {
		ret = -errno;
		write_log(0, "Error when renaming meta to to_delete\n");
		return ret;
	}

	FWRITE(&thisinode, sizeof(ino_t), 1, to_delete_fptr);

	return 0;
errcode_handle:
	return errcode;
}

int32_t delete_meta_blocks(ino_t thisinode, BOOL delete_block)
{
	char fetchedmeta[METAPATHLEN];
	char thisblockpath[BLOCKPATHLEN];
	int32_t ret, errcode;
	FILE *metafptr = NULL;
	HCFS_STAT this_inode_stat;
	FILE_META_TYPE file_meta;
	BLOCK_ENTRY_PAGE tmppage;
	size_t ret_size;
	int64_t total_blocks;
	int64_t current_page;
	int64_t count;
	int64_t e_index, which_page;
	int64_t page_pos;
	char block_status;
	int64_t total_removed_cache_size = 0;
	int64_t total_removed_cache_blks = 0;
	struct stat cache_stat;
	struct stat meta_stat;
	int64_t metasize, metasize_blk;
	int64_t est_pin_size, real_pin_size;

	fetch_restore_meta_path(fetchedmeta, thisinode);
	if (access(fetchedmeta, F_OK) != 0)
		return 0;

	if (delete_block == FALSE) {
		ret = stat(fetchedmeta, &meta_stat);
		if (ret < 0) {
			errcode = -errno;
			write_log(0, "Unable to stat restored meta\n");
			goto errcode_handle;
		}
		metasize = meta_stat.st_size;
		metasize_blk = meta_stat.st_blocks * 512;

 /*
  * Backend statistics won't be adjusted here, as they will be updated
  * when backend objects are deleted
  */

		if (use_old_cloud_stat == TRUE) {
			UPDATE_RECT_SYSMETA(.delta_system_size = 0,
					    .delta_meta_size =
						metasize_blk -
						(metasize + 4096),
					    .delta_pinned_size = 0,
					    .delta_backend_size = 0,
					    .delta_backend_meta_size = 0,
					    .delta_backend_inodes = 0);

			UPDATE_RESTORE_SYSMETA(.delta_system_size = -metasize,
					       .delta_meta_size =
						   -(metasize + 4096),
					       .delta_pinned_size = 0,
					       .delta_backend_size = 0,
					       .delta_backend_meta_size = 0,
					       .delta_backend_inodes = 0);
		} else {
			UPDATE_RESTORE_SYSMETA(.delta_system_size = -metasize,
					       .delta_meta_size = -metasize_blk,
					       .delta_pinned_size = 0,
					       .delta_backend_size = 0,
					       .delta_backend_meta_size = 0,
					       .delta_backend_inodes = 0);
		}

		/*
		 * FEATURE TODO: Now file meta size will be substracted
		 * from total meta size as soon as the file is deleted,
		 * but before the meta is actually deleted in to_delete
		 * folder.  The computation here will follow the current
		 * implementation, but this should be changed later to
		 * reflect the actual meta size
		 */
		/* Mark this inode as to delete */
		ret = _mark_delete(thisinode);
		return ret;
	}

	metafptr = fopen(fetchedmeta, "r");
	if (metafptr == NULL) {
		errcode = -errno;
		write_log(4, "Cannot read meta for block deletion.\n");
		return errcode;
	}

	FREAD(&this_inode_stat, sizeof(HCFS_STAT), 1, metafptr);
	if (ret_size < 1) {
		write_log(2, "Skipping block deletion (meta gone)\n");
		fclose(metafptr);
		return -EIO;
	}

	FREAD(&file_meta, sizeof(FILE_META_TYPE), 1, metafptr);
	if (ret_size < 1) {
		write_log(2, "Skipping block deletion (meta gone)\n");
		fclose(metafptr);
		return -EIO;
	}

	if (P_IS_PIN(file_meta.local_pin)) {
		real_pin_size = round_size(this_inode_stat.size);
		if (use_old_cloud_stat == TRUE)
			est_pin_size = (this_inode_stat.size + 4096);
		else
			est_pin_size = real_pin_size;
	} else {
		real_pin_size = 0;
		est_pin_size = 0;
	}

	ret = stat(fetchedmeta, &meta_stat);
	if (ret < 0) {
		errcode = -errno;
		write_log(0, "Unable to stat restored meta\n");
		goto errcode_handle;
	}
	metasize = meta_stat.st_size;
	metasize_blk = meta_stat.st_blocks * 512;

	/*
	 * Backend statistics won't be adjusted here, as they will be updated
	 * when backend objects are deleted
	 */
	if (use_old_cloud_stat == TRUE) {
		UPDATE_RECT_SYSMETA(.delta_system_size = 0,
				    .delta_meta_size =
					metasize_blk - (metasize + 4096),
				    .delta_pinned_size =
					real_pin_size - est_pin_size,
				    .delta_backend_size = 0,
				    .delta_backend_meta_size = 0,
				    .delta_backend_inodes = 0);

		UPDATE_RESTORE_SYSMETA(.delta_system_size = -metasize,
				       .delta_meta_size = -(metasize + 4096),
				       .delta_pinned_size = -est_pin_size,
				       .delta_backend_size = 0,
				       .delta_backend_meta_size = 0,
				       .delta_backend_inodes = 0);
	} else {
		UPDATE_RESTORE_SYSMETA(.delta_system_size = -metasize,
				       .delta_meta_size = -metasize_blk,
				       .delta_pinned_size = -real_pin_size,
				       .delta_backend_size = 0,
				       .delta_backend_meta_size = 0,
				       .delta_backend_inodes = 0);
	}
	/*
	 * FEATURE TODO: Now file meta size will be substracted from
	 * total meta size as soon as the file is deleted, but before the
	 * meta is actually deleted in to_delete folder.  The computation
	 * here will follow the current implementation, but this should
	 * be changed later to reflect the actual meta size
	 */

	if (this_inode_stat.size == 0)
		total_blocks = 0;
	else
		total_blocks =
		    ((this_inode_stat.size - 1) / MAX_BLOCK_SIZE) + 1;

	current_page = -1;
	for (count = 0; count < total_blocks; count++) {
		e_index = count % MAX_BLOCK_ENTRIES_PER_PAGE;
		which_page = count / MAX_BLOCK_ENTRIES_PER_PAGE;

		if (current_page != which_page) {
			page_pos =
			    seek_page2(&file_meta, metafptr, which_page, 0);
			if (page_pos <= 0) {
				count += (MAX_BLOCK_ENTRIES_PER_PAGE - 1);
				continue;
			}
			current_page = which_page;
			FSEEK(metafptr, page_pos, SEEK_SET);
			memset(&tmppage, 0, sizeof(BLOCK_ENTRY_PAGE));
			FREAD(&tmppage, sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
		}

		/* Skip if block does not exist */
		block_status = tmppage.block_entries[e_index].status;
		if ((block_status == ST_NONE) || (block_status == ST_CLOUD))
			continue;

		ret = fetch_restore_block_path(thisblockpath, thisinode, count);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}

		if (access(thisblockpath, F_OK) == 0) {
			ret = stat(thisblockpath, &cache_stat);
			if (ret == 0)
				total_removed_cache_size +=
				    cache_stat.st_blocks * 512;
			total_removed_cache_blks += 1;
			UNLINK(thisblockpath);
		}
	}
	fclose(metafptr);
	metafptr = NULL;

	update_restored_cache_usage(-total_removed_cache_size,
				    -total_removed_cache_blks);

	/* Mark this inode as to delete */
	ret = _mark_delete(thisinode);

	return ret;
errcode_handle:
	if (metafptr != NULL)
		fclose(metafptr);
	return errcode;
}


/*
 * Helper for pruning meta and data files of missing or deleted folders
 */
int32_t _recursive_prune(ino_t thisinode)
{
	FILE *fptr;
	char fetchedmeta[METAPATHLEN];
	DIR_META_TYPE dirmeta;
	DIR_ENTRY_PAGE tmppage;
	int64_t filepos;
	int32_t count;
	ino_t tmpino;
	DIR_ENTRY *tmpptr;
	int32_t ret, errcode;
	size_t ret_size;

	fetch_restore_meta_path(fetchedmeta, thisinode);
	if (access(fetchedmeta, F_OK) != 0)
		return 0;
	fptr = fopen(fetchedmeta, "r");
	if (fptr == NULL) {
		write_log(0, "Error when reading files to prune\n");
		errcode = -errno;
		return errcode;
	}

	setbuf(fptr, NULL);
	FSEEK(fptr, sizeof(HCFS_STAT), SEEK_SET);
	FREAD(&dirmeta, sizeof(DIR_META_TYPE), 1, fptr);

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

			write_log(10, "Pruning %s\n", tmpptr->d_name);

			tmpino = tmpptr->d_ino;
			switch (tmpptr->d_type) {
			case D_ISLNK:
				/* Just delete the meta */
				ret = delete_meta_blocks(tmpino, FALSE);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}
				break;
			case D_ISREG:
			case D_ISFIFO:
			case D_ISSOCK:
				/* Delete the blocks and meta */
				ret = delete_meta_blocks(tmpino, TRUE);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}
				break;
			case D_ISDIR:
				/* Need to expand */
				ret = _recursive_prune(tmpino);
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
	unlink(fetchedmeta);
	return 0;
errcode_handle:
	fclose(fptr);
	return errcode;
}
/* Helper function for pruning dead files / apps from FS */
int32_t _prune_missing_entries(ino_t thisinode,
			       PRUNE_T *prune_list,
			       int32_t prune_num)
{
	int32_t count, ret, errcode;
	char fetchedmeta[METAPATHLEN];
	char tmppath[METAPATHLEN];
	DIR_META_TYPE parent_meta;
	DIR_ENTRY tmpentry;
	HCFS_STAT parent_stat;
	DIR_ENTRY_PAGE tpage;
	DIR_ENTRY temp_dir_entries[2 * (MAX_DIR_ENTRIES_PER_PAGE + 2)];
	int64_t temp_child_page_pos[2 * (MAX_DIR_ENTRIES_PER_PAGE + 3)];
	FILE *fptr = NULL;
	size_t ret_size;
	struct stat tmpmeta_struct;
	int64_t old_metasize, new_metasize;
	int64_t old_metasize_blk, new_metasize_blk;

	fetch_restore_meta_path(fetchedmeta, thisinode);
	fptr = fopen(fetchedmeta, "r+");
	if (fptr == NULL) {
		write_log(0, "Error when fetching file to restore\n");
		errcode = -errno;
		return errcode;
	}
	setbuf(fptr, NULL);

	fstat(fileno(fptr), &tmpmeta_struct);
	old_metasize = (int64_t)tmpmeta_struct.st_size;
	old_metasize_blk = (int64_t)tmpmeta_struct.st_blocks * 512;

	FSEEK(fptr, 0, SEEK_SET);
	FREAD(&parent_stat, sizeof(HCFS_STAT), 1, fptr);
	FREAD(&parent_meta, sizeof(DIR_META_TYPE), 1, fptr);

	for (count = 0; count < prune_num; count++) {
		if (hcfs_system->system_going_down == TRUE) {
			errcode = -ESHUTDOWN;
			goto errcode_handle;
		}
		fetch_restore_meta_path(tmppath, prune_list[count].entry.d_ino);
		write_log(10, "Processing removal of entry %s\n",
			  prune_list[count].entry.d_name);
		if (access(tmppath, F_OK) == 0) {
			/* Delete everything inside recursively */
			ret = _recursive_prune(prune_list[count].entry.d_ino);
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}
		}
		/* Need to remove entry from meta */
		memcpy(&tmpentry, &(prune_list[count].entry),
		       sizeof(DIR_ENTRY));

		/*
		 * Initialize B-tree deletion by first loading the
		 * root of B-tree
		 */
		memset(&tpage, 0, sizeof(DIR_ENTRY_PAGE));
		memset(temp_dir_entries, 0,
		       sizeof(DIR_ENTRY) *
			   (2 * (MAX_DIR_ENTRIES_PER_PAGE + 2)));
		memset(temp_child_page_pos, 0,
		       sizeof(int64_t) * (2 * (MAX_DIR_ENTRIES_PER_PAGE + 3)));
		tpage.this_page_pos = parent_meta.root_entry_page;

		/* Read root node */
		FSEEK(fptr, parent_meta.root_entry_page, SEEK_SET);
		FREAD(&tpage, sizeof(DIR_ENTRY_PAGE), 1, fptr);

		/* Recursive B-tree deletion routine*/
		ret = delete_dir_entry_btree(&tmpentry, &tpage, fileno(fptr),
					     &parent_meta, temp_dir_entries,
					     temp_child_page_pos, FALSE);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}

		write_log(10, "delete dir entry returns %d\n", ret);

		/*
		 * If the entry is a subdir, decrease the hard link of
		 * the parent
		 */

		if (tmpentry.d_type == D_ISDIR)
			parent_stat.nlink--;

		parent_meta.total_children--;
		write_log(10, "TOTAL CHILDREN is now %lld\n",
			  parent_meta.total_children);
		set_timestamp_now(&parent_stat, MTIME | CTIME);

		FSEEK(fptr, 0, SEEK_SET);
		FWRITE(&parent_stat, sizeof(HCFS_STAT), 1, fptr);
		FWRITE(&parent_meta, sizeof(DIR_META_TYPE), 1, fptr);
	}

	fstat(fileno(fptr), &tmpmeta_struct);
	new_metasize = (int64_t)tmpmeta_struct.st_size;
	new_metasize_blk = (int64_t)tmpmeta_struct.st_blocks * 512;

	fclose(fptr);

	UPDATE_RESTORE_SYSMETA(.delta_system_size = new_metasize - old_metasize,
			       .delta_meta_size =
				   new_metasize_blk - old_metasize_blk,
			       .delta_pinned_size = 0, .delta_backend_size = 0,
			       .delta_backend_meta_size = 0,
			       .delta_backend_inodes = 0);

	/* Mark this inode to to_sync */
	FWRITE(&thisinode, sizeof(ino_t), 1, to_sync_fptr);
	return 0;
errcode_handle:
	write_log(0, "Unable to prune missing entries in restoration. (%" PRIu64
		     ")\n",
		  thisinode);
	fclose(fptr);
	return errcode;
}

static inline void _realloc_prune(PRUNE_T **prune_list, int32_t *max_prunes)
{
	PRUNE_T *tmp_prune_ptr;

	tmp_prune_ptr = (PRUNE_T *)realloc(*prune_list, (*max_prunes + 10) *
							    sizeof(PRUNE_T));
	if (tmp_prune_ptr == NULL)
		return;
	*prune_list = tmp_prune_ptr;
	*max_prunes += 10;
}

int32_t _replace_missing_pinned(ino_t srcinode, ino_t thisinode)
{
	FILE_META_TYPE tmpmeta;
	HCFS_STAT tmpstat;
	FILE *fptr;
	char metapath[METAPATHLEN];
	int64_t count = 0, totalblocks, tmpsize, blkcount;
	int64_t nowpage, lastpage, filepos, nowindex;
	int64_t num_cached_block = 0, cached_size = 0;
	int32_t errcode, ret;
	size_t ret_size;
	struct stat blockstat;
	BLOCK_ENTRY_PAGE temppage;
	FILE_STATS_TYPE file_stats_type;
	BOOL write_page;
	char blockpath[BLOCKPATHLEN];
	char srcblockpath[BLOCKPATHLEN];
	int64_t file_stats_pos;

	fetch_restore_meta_path(metapath, thisinode);
	fptr = fopen(metapath, "r+");
	if (fptr == NULL) {
		write_log(0, "Error when fetching file to restore\n");
		errcode = -errno;
		return errcode;
	}
	setbuf(fptr, NULL);

	FSEEK(fptr, 0, SEEK_SET);
	FREAD(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
	/* Let link number be 1 */
	tmpstat.nlink = 1;
	FSEEK(fptr, 0, SEEK_SET);
	FWRITE(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
	FSEEK(fptr, sizeof(HCFS_STAT), SEEK_SET);
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
				FWRITE(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1,
				       fptr);
				write_page = FALSE;
			}
			/* Reload page pos */
			filepos = seek_page2(&tmpmeta, fptr, nowpage, 0);
			if (filepos < 0) {
				errcode = (int32_t)filepos;
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
			fetch_restore_block_path(blockpath, thisinode, count);
			fetch_block_path(srcblockpath, srcinode, count);
			/* Copy block from source */
			ret = copy_file(srcblockpath, blockpath);
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}
			/* Change block status in meta */
			temppage.block_entries[nowindex].status = ST_LDISK;
			write_page = TRUE;
			/* Update file stats */
			ret = stat(blockpath, &blockstat);
			if (ret == 0) {
				cached_size += blockstat.st_blocks * 512;
				num_cached_block += 1;
			} else {
				write_log(0, "%s %s. Code %d",
					  "Error: Fail to stat block in",
					  __func__, errno);
			}
		}
	}
	if (write_page == TRUE) {
		FSEEK(fptr, filepos, SEEK_SET);
		FWRITE(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
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
	if (errcode == -ENOENT) {
		write_log(4, "Cleaning up blocks of broken file\n");
		for (blkcount = 0; blkcount <= count; blkcount++) {
			fetch_restore_block_path(blockpath, thisinode,
						 blkcount);
			unlink(blockpath);
		}
	}

	write_log(0, "Restore Error: Code %d\n", -errcode);
	write_log(10, "Error in %s.\n", __func__);
	return errcode;
}

ino_t _stage1_get_new_inode(void)
{
	ino_t ret_ino;

	LOCK_RESTORED_SYSMETA();
	ret_ino = hcfs_restored_system_meta->system_max_inode + 1;
	hcfs_restored_system_meta->system_max_inode += 1;
	UNLOCK_RESTORED_SYSMETA();

	return ret_ino;
}

int32_t _check_hardlink(ino_t src_inode, ino_t *target_inode,
		BOOL *need_copy, INODE_PAIR_LIST *hardln_mapping)
{
	char srcpath[METAPATHLEN], targetpath[METAPATHLEN];
	int32_t ret, errcode;
	HCFS_STAT tmpstat;
	int64_t ret_size;
	FILE *fptr;

	if (hardln_mapping == NULL)
		return -ENOMEM;

	fetch_meta_path(srcpath, src_inode);
	fptr = fopen(srcpath, "r");
	if (!fptr) {
		errcode = -errno;
		return errcode;
	}
	flock(fileno(fptr), LOCK_EX);
	FSEEK(fptr, 0, SEEK_SET);
	FREAD(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	if (tmpstat.nlink < 1) {
		return -ENOENT;
	} else if (tmpstat.nlink == 1) {
		*target_inode = _stage1_get_new_inode();
		*need_copy = TRUE;
	} else {
		/* Find hardlink target inode */
		ret = find_target_inode(hardln_mapping, src_inode,
				target_inode);
		if (ret < 0) {
			/* If not found, get a new inode number */
			*target_inode = _stage1_get_new_inode();
			insert_inode_pair(hardln_mapping,
				src_inode, *target_inode);
			*need_copy = TRUE;
		} else {
			/* Otherwise, use the hardlink inode number and
			 * increase nlink */
			fetch_restore_meta_path(targetpath, *target_inode);
			fptr = fopen(targetpath, "r+");
			if (!fptr) {
				errcode = -errno;
				return errcode;
			}
			flock(fileno(fptr), LOCK_EX);
			FSEEK(fptr, 0, SEEK_SET);
			FREAD(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
			tmpstat.nlink += 1;
			FSEEK(fptr, 0, SEEK_SET);
			FWRITE(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
			flock(fileno(fptr), LOCK_UN);
			fclose(fptr);
			*need_copy = FALSE;
		}
	}
	write_log(0, "Test: inode mapping: src %"PRIu64", target %"PRIu64,
			(uint64_t)src_inode, (uint64_t)(*target_inode));

	return 0;

errcode_handle:
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	return errcode;
}

/**
 * Replace missing object under restored folder /data/data. Object can
 * be any type. "src_inode" is the inode number corresponding to now system.
 * The file meta_<target_inode> under restored metastorage will be replaced
 * with file meta_<src_inode> under now system metastorage.
 *
 * @param src_inode Source inode number whose meta file will be copied.
 * @param target_inode Target inode number whose meta file is copied
 *                     from meta of src_inode.
 * @param type Type of target meta file.
 *
 * @return 0 on success. Otherwise return negation of error number.
 */
int32_t replace_missing_object(ino_t src_inode, ino_t target_inode, char type,
		int32_t uid, INODE_PAIR_LIST *hardln_mapping)
{
	FILE *fptr;
	char srcpath[METAPATHLEN], targetpath[METAPATHLEN];
	int32_t ret, errcode, idx;
	int32_t list_counter, list_size;
	int64_t now_page_pos;
	int64_t ret_size;
	ino_t child_src_inode, child_target_inode;
	DIR_META_TYPE dirmeta;
	DIR_ENTRY_PAGE dir_page;
	DIR_ENTRY *removed_list = NULL;
	DIR_ENTRY temp_dir_entries[2*(MAX_DIR_ENTRIES_PER_PAGE+2)];
	int64_t temp_child_page_pos[(MAX_DIR_ENTRIES_PER_PAGE+3)];
	HCFS_STAT dirstat, tmpstat;
	BOOL meta_open = FALSE;
	BOOL need_copy;
	char now_type;

	/* Skip socket and fifo? */
	if (type == D_ISSOCK || type == D_ISFIFO)
		return -ENOENT;

	fetch_meta_path(srcpath, src_inode);
	fetch_restore_meta_path(targetpath, target_inode);

	/* Copy from now system */
	ret = copy_file(srcpath, targetpath);
	if (ret < 0)
		return ret;
	fptr = fopen(targetpath, "r+");
	if (fptr == NULL) {
		ret = -errno;
		write_log(0, "Error: Fail to open file. Code %d", -ret);
		return ret;
	}
	meta_open = TRUE;
	/* Check type */
	FSEEK(fptr, 0, SEEK_SET);
	FREAD(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
	if (S_ISREG(tmpstat.mode) && type == D_ISREG)
		ret = 0;
	else if (S_ISDIR(tmpstat.mode) && type == D_ISDIR)
		ret = 0;
	else if (S_ISLNK(tmpstat.mode) && type == D_ISLNK)
		ret = 0;
	else
		ret = -ENOENT;
	if (ret < 0) {
		write_log(0, "Error: Type does not match");
		errcode = ret;
		goto errcode_handle;
	}
	setbuf(fptr, NULL);

	/* Case regular file */
	if (type == D_ISREG || type == D_ISLNK) {
		errcode = restore_borrowed_meta_structure(fptr,
				uid, src_inode, target_inode);
		if (errcode < 0)
			goto errcode_handle;
		fclose(fptr);
		meta_open = FALSE;
		/* Mark this inode to to_sync */
		FWRITE(&target_inode, sizeof(ino_t), 1, to_sync_fptr);
		return 0;
	}

	errcode = restore_borrowed_meta_structure(fptr, uid,
			src_inode, target_inode);
	if (errcode < 0)
		goto errcode_handle;

	/* Recursively copy dir */
	list_counter = 0;
	list_size = 20;
	removed_list = malloc(sizeof(DIR_ENTRY) * list_size);
	FSEEK(fptr, sizeof(HCFS_STAT), SEEK_SET);
	FREAD(&dirmeta, sizeof(DIR_META_TYPE), 1, fptr);
	now_page_pos = dirmeta.tree_walk_list_head;
	while (now_page_pos) {
		DIR_ENTRY *now_entry;

		FSEEK(fptr, now_page_pos, SEEK_SET);
		FREAD(&dir_page, sizeof(DIR_ENTRY_PAGE), 1, fptr);
		for (idx = 0; idx < dir_page.num_entries; idx++) {
			now_entry = &(dir_page.dir_entries[idx]);
			if (strcmp(now_entry->d_name, ".") == 0 ||
					strcmp(now_entry->d_name, "..") == 0)
				continue;
			child_src_inode = now_entry->d_ino;
			now_type = now_entry->d_type;
			if (now_type == D_ISREG || now_type == D_ISLNK) {
				/* Check if it is a hardlink, and then
				 * get a target inode */
				need_copy = TRUE;
				ret = _check_hardlink(child_src_inode,
					&child_target_inode, &need_copy,
					hardln_mapping);
				if (ret == 0) {
					if (need_copy == FALSE) {
						/* Hardlink exists */
						now_entry->d_ino =
							child_target_inode;
						continue;
					}
					ret = replace_missing_object(
						child_src_inode,
						child_target_inode,
						now_entry->d_type,
						uid, hardln_mapping);
					write_log(0, "Test: Replacing with %s"
						, now_entry->d_name);
				}

			} else if (now_type == D_ISDIR) {
				child_target_inode = _stage1_get_new_inode();
				/* Recursively replace all entries */
				ret = replace_missing_object(child_src_inode,
					child_target_inode, now_entry->d_type,
					uid, hardln_mapping);
				write_log(0, "Test: Replacing with %s"
						, now_entry->d_name);

			} else {
				ret = -ENOENT;
			}

			if (ret < 0) {
				/* Add to removed entry */
				memcpy(&(removed_list[list_counter]),
					now_entry, sizeof(DIR_ENTRY));
				list_counter++;
				if (list_counter >= list_size) {
					list_size += 20;
					removed_list = realloc(removed_list,
						list_size);
					if (removed_list == NULL) {
						errcode = -errno;
						goto errcode_handle;
					}
				}
			} else {
				write_log(0, "Test: Replacing with %s is"
					" successful", now_entry->d_name);
				now_entry->d_ino = child_target_inode;
			}
		}
		FSEEK(fptr, now_page_pos, SEEK_SET);
		FWRITE(&dir_page, sizeof(DIR_ENTRY_PAGE), 1, fptr);
		now_page_pos = dir_page.tree_walk_next;
	}

	/* Remove missing entries */
	FSEEK(fptr, 0, SEEK_SET);
	FREAD(&dirstat, sizeof(HCFS_STAT), 1, fptr);
	for (idx = 0; idx < list_counter; idx++) {
		memset(temp_dir_entries, 0,
		       sizeof(DIR_ENTRY) * (2*(MAX_DIR_ENTRIES_PER_PAGE+2)));
		memset(temp_child_page_pos, 0,
		       sizeof(int64_t) * (2*(MAX_DIR_ENTRIES_PER_PAGE+3)));

		FSEEK(fptr, dirmeta.root_entry_page, SEEK_SET);
		FREAD(&dir_page, sizeof(DIR_ENTRY_PAGE), 1, fptr);
		ret = delete_dir_entry_btree(&(removed_list[idx]),
			&dir_page, fileno(fptr), &dirmeta,
			temp_dir_entries, temp_child_page_pos, FALSE);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}

		/*
		 * If the entry is a subdir, decrease the hard link of
		 * the parent.
		 */
		if (removed_list[idx].d_type == D_ISDIR)
			dirstat.nlink--;
		dirmeta.total_children--;
		set_timestamp_now(&dirstat, MTIME | CTIME);
		FSEEK(fptr, 0, SEEK_SET);
		FWRITE(&dirstat, sizeof(HCFS_STAT), 1, fptr);
		FWRITE(&dirmeta, sizeof(DIR_META_TYPE), 1, fptr);
	}
	fclose(fptr);
	meta_open = FALSE;
	FREE(removed_list);

	/* Mark this inode to to_sync */
	FWRITE(&target_inode, sizeof(ino_t), 1, to_sync_fptr);

	return 0;

errcode_handle:
	if (meta_open)
		fclose(fptr);
	unlink(targetpath);
	FREE(removed_list);
	return errcode;
}

void _extract_pkg_name(const char *srcpath, char *pkgname)
{
	while (*srcpath != '/' && *srcpath != '\0') {
		*pkgname = *srcpath;
		srcpath++;
		pkgname++;
	}
	*pkgname = '\0';
}

int32_t replace_missing_meta(const char *nowpath, DIR_ENTRY *tmpptr,
		INODE_PAIR_LIST *hardln_mapping)
{
	char pkg[MAX_FILENAME_LEN + 1];
	char tmppath[PATH_MAX];
	int32_t uid, ret, errcode;
	ino_t src_inode;
	struct stat tmpstat;
	FILE *fptr;
	int64_t ret_size;

	/* Skip to copy socket and fifo */
	if (tmpptr->d_type == D_ISSOCK || tmpptr->d_type == D_ISFIFO)
		return -ENOENT;

	if (strlen(nowpath) == strlen("/data/data"))
		strcpy(pkg, tmpptr->d_name);
	else
		_extract_pkg_name(nowpath + strlen("/data/data/"), pkg);
	write_log(0, "Test: Pkg name %s", pkg);
	uid = lookup_package_uid_list(pkg);

	snprintf(tmppath, PATH_MAX, "%s/%s", nowpath, tmpptr->d_name);
	ret = stat(tmppath, &tmpstat);
	if (ret < 0 || uid < 0) {
		write_log(0, "Error: Cannot use %s meta. Uid %d",
				tmppath, uid);
		return -ENOENT;
	}
	src_inode = tmpstat.st_ino;

	if (hardln_mapping == NULL)
		return -ENOMEM;

	write_log(4, "Replacing %s with the copy on the device\n", nowpath);
	/* Do not need to check hardlink for folder */
	if (tmpptr->d_type == D_ISDIR) {
		ret = replace_missing_object(src_inode, tmpptr->d_ino,
				tmpptr->d_type, uid, hardln_mapping);
		return ret;
	}

	/* Check link number for regfile and symlink */
	if (tmpstat.st_nlink < 1) {
		return -ENOENT;
	} else if (tmpstat.st_nlink == 1) {
		ret = replace_missing_object(src_inode, tmpptr->d_ino,
				tmpptr->d_type, uid, hardln_mapping);
		if (ret < 0)
			return ret;
	} else {
		ino_t target_inode;
		char targetpath[MAX_FILENAME_LEN];
		HCFS_STAT hcfsstat;

		write_log(4, "Warn: Detect missing hardlink %s", tmppath);
		/* Find hardlink target inode */
		ret = find_target_inode(hardln_mapping, src_inode,
				&target_inode);
		if (ret < 0) {
			/* If not found, create the hardlink mapping */
			ret = replace_missing_object(src_inode, tmpptr->d_ino,
					tmpptr->d_type, uid, hardln_mapping);
			if (ret < 0)
				return ret;
			insert_inode_pair(hardln_mapping,
				src_inode, tmpptr->d_ino);
		} else {
			/* Otherwise, use the hardlink inode number and
			 * increase nlink */
			fetch_restore_meta_path(targetpath, target_inode);
			fptr = fopen(targetpath, "r+");
			if (!fptr) {
				errcode = -errno;
				write_log(0, "Error: Fail to open file in %s."
					" Code %d", __func__, -errcode);
				return errcode;
			}
			flock(fileno(fptr), LOCK_EX);
			FSEEK(fptr, 0, SEEK_SET);
			FREAD(&hcfsstat, sizeof(HCFS_STAT), 1, fptr);
			hcfsstat.nlink += 1;
			FSEEK(fptr, 0, SEEK_SET);
			FWRITE(&hcfsstat, sizeof(HCFS_STAT), 1, fptr);
			flock(fileno(fptr), LOCK_UN);
			fclose(fptr);
			/* Remember to modify the inode number in
			 * parent folder */
			tmpptr->d_ino = target_inode;
		}
	}

	return 0;

errcode_handle:
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	return errcode;
}

static int32_t _update_packages_list(PRUNE_T *prune_list, int32_t num_prunes);
int32_t _expand_and_fetch(ino_t thisinode, char *nowpath, int32_t depth,
		INODE_PAIR_LIST *hardln_mapping)
{
	FILE *fptr;
	char fetchedmeta[METAPATHLEN];
	char tmppath[PATH_MAX];
	DIR_META_TYPE dirmeta;
	DIR_ENTRY_PAGE tmppage;
	int64_t filepos;
	int32_t count;
	ino_t tmpino;
	DIR_ENTRY *tmpptr;
	int32_t expand_val;
	BOOL skip_this, can_prune = FALSE;
	int32_t ret, errcode;
	size_t ret_size;
	PRUNE_T *prune_list = NULL;
	int32_t prune_index = 0, max_prunes = 0;
	BOOL object_replace;

	fetch_restore_meta_path(fetchedmeta, thisinode);
	fptr = fopen(fetchedmeta, "r");
	if (fptr == NULL) {
		write_log(0, "Error when fetching file to restore\n");
		errcode = -errno;
		return errcode;
	}

	setbuf(fptr, NULL);
	FSEEK(fptr, sizeof(HCFS_STAT), SEEK_SET);
	FREAD(&dirmeta, sizeof(DIR_META_TYPE), 1, fptr);

	/* Do not expand if not high priority pin and not needed */
	expand_val = 1; /* The default */
	if (dirmeta.local_pin != P_HIGH_PRI_PIN) {
		expand_val = _check_expand(thisinode, nowpath, depth);
		if (expand_val == 0)
			return 0;
		if (expand_val == 5)
			can_prune = TRUE;
	} else {
		if (strncmp(nowpath, "/data/app", strlen("/data/app")) == 0)
			can_prune = TRUE;
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
			object_replace = FALSE;
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

			/*
			 * For high-priority pin dirs in /data/app, if
			 * missing, could just prune the app out (will
			 * need to verify though).
			 */
			/* First fetch the meta */
			tmpino = tmpptr->d_ino;
			ret = _fetch_meta(tmpino);
			if ((ret == -ENOENT) && (can_prune == TRUE)) {
				/*
				 * Handle app pruning for missing files
				 * in /data/app here. First check for the
				 * type of missing element
				 */
				if ((depth != 1) ||
				    (strcmp("base.apk", tmpptr->d_name) != 0)) {
					/* Just remove the element */
					if (prune_index >= max_prunes)
						_realloc_prune(&prune_list,
							       &max_prunes);
					if (prune_index >= max_prunes) {
						errcode = -ENOMEM;
						free(prune_list);
						goto errcode_handle;
					}
					memcpy(&(prune_list[prune_index].entry),
					       tmpptr, sizeof(DIR_ENTRY));
					prune_index++;
					write_log(
					    4, "%s gone from %s. Removing.\n",
					    tmpptr->d_name, nowpath);
				} else {
					/*
					 * Remove the entire app folder.
					 * Raise the error and catch it
					 * later at /data/app level
					 */
					errcode = ret;
					goto errcode_handle;
				}

				continue;
			}
			if (ret < 0) {
				can_prune = FALSE;
				if (((ret == -ENOENT) && (expand_val == 1)) &&
				    (strncmp(nowpath, "/data/data",
					     strlen("/data/data")) == 0)) {

					ret = replace_missing_meta(nowpath,
						tmpptr, hardln_mapping);
					/* Socket and fifo file will be
					 * pruned */
					if (ret < 0)
						can_prune = TRUE;
					else
						object_replace = TRUE;
				}
				if (can_prune == TRUE) {
					if (prune_index >= max_prunes)
						_realloc_prune(&prune_list,
							       &max_prunes);
					if (prune_index >= max_prunes) {
						errcode = -ENOMEM;
						free(prune_list);
						goto errcode_handle;
					}
					memcpy(&(prune_list[prune_index].entry),
					       tmpptr, sizeof(DIR_ENTRY));
					prune_index++;
					write_log(
					    4, "%s gone from %s. Removing.\n",
					    tmpptr->d_name, nowpath);
					continue;
				} else if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}
			}
			/* Skip to fetch data from cloud if file is
			 * replaced with local data */
			if (object_replace == TRUE)
				continue;

			/* If meta exist, fetch data or expand dir */
			switch (tmpptr->d_type) {
			case D_ISLNK:
				/* Just fetch the meta */
				break;
			case D_ISREG:
			case D_ISFIFO:
			case D_ISSOCK:
				/* Fetch all blocks if pinned */
				can_prune = FALSE;
				ret = _fetch_pinned(tmpino);
				if (((ret == -ENOENT) && (expand_val == 1)) &&
				    ((tmpptr->d_type == D_ISREG) &&
				     (strncmp(nowpath, "/data/data",
					      strlen("/data/data")) == 0))) {
					ret = replace_missing_meta(nowpath,
						tmpptr, hardln_mapping);
					if (ret < 0)
						can_prune = TRUE;
				}
				if (can_prune == TRUE) {
					if (prune_index >= max_prunes)
						_realloc_prune(&prune_list,
							       &max_prunes);
					if (prune_index >= max_prunes) {
						errcode = -ENOMEM;
						free(prune_list);
						goto errcode_handle;
					}
					memcpy(&(prune_list[prune_index].entry),
					       tmpptr, sizeof(DIR_ENTRY));
					prune_index++;
					write_log(
					    4, "%s gone from %s. Removing.\n",
					    tmpptr->d_name, nowpath);
				} else if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}
				break;
			case D_ISDIR:
				/* Need to expand */
				snprintf(tmppath, PATH_MAX, "%s/%s", nowpath,
					 tmpptr->d_name);
				ret = _expand_and_fetch(tmpino, tmppath,
						depth + 1, hardln_mapping);
				if ((ret == -ENOENT) &&
				    (strcmp(nowpath, "/data/app") == 0)) {
					/* Need to prune the package */
					if (prune_index >= max_prunes)
						_realloc_prune(&prune_list,
							       &max_prunes);
					if (prune_index >= max_prunes) {
						errcode = -ENOMEM;
						free(prune_list);
						goto errcode_handle;
					}
					memcpy(&(prune_list[prune_index].entry),
					       tmpptr, sizeof(DIR_ENTRY));
					prune_index++;
					write_log(
					    4, "%s gone from %s. Removing.\n",
					    tmpptr->d_name, nowpath);
				} else if (ret < 0) {
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
	if (prune_index > 0)
		_prune_missing_entries(thisinode, prune_list, prune_index);

	/*
	 * If deleting app folders from /data/app, need to set version to
	 * zero in packages.xml
	 */
	if ((prune_index > 0) && (strcmp(nowpath, "/data/app") == 0)) {
		write_log(2, "Some apps are missing binaries. Removing.\n");
		errcode = _update_packages_list(prune_list, prune_index);
		if (errcode < 0) {
			free(prune_list);
			return errcode;
		}
	}

	free(prune_list);
	return 0;

errcode_handle:
	fclose(fptr);
	free(prune_list);
	return errcode;
}

/*
 * This is a helper function for writing reconstructed system stat to
 * hcfssystemfile in restored meta storage folder
 */
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

	/*
	 * Backup rectified space usage, which is used to re-compute
	 * system usage in restoration stage 2.
	 */
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
	}

	download_usermeta_ctl.active = TRUE;
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

void _init_quota_restore(void)
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

static void _replace_version(char *fbuf, int32_t initpos, int32_t fbuflen)
{
	int32_t startpos, endpos;

	startpos = initpos;
	while (startpos < fbuflen) {
		while ((fbuf[startpos] != ' ') && (startpos < fbuflen))
			startpos++;
		if (startpos >= fbuflen)
			break;
		/* Start of another field */
		startpos++;
		endpos = startpos;
		while (((fbuf[endpos] != '=') && (fbuf[endpos] != ' ')) &&
		       (endpos < fbuflen))
			endpos++;
		if ((endpos >= fbuflen) || ((endpos - startpos) > 255))
			break;
		if (fbuf[endpos] == ' ') { /* This might be the startpos */
			startpos = endpos;
			continue;
		}
		/* Check if this is the version field */
		if (strncmp(&(fbuf[startpos]), "version",
			    (endpos - startpos)) != 0) {
			/* Not the field, continue */
			startpos = endpos;
			continue;
		}
		/* Mark the start and the end of the value */
		startpos = endpos;
		while ((fbuf[startpos] != '"') && (startpos < fbuflen))
			startpos++;
		if (startpos >= fbuflen)
			break;
		startpos++;
		endpos = startpos;
		while ((fbuf[endpos] != '"') && (endpos < fbuflen))
			endpos++;
		/* terminate if no value or no valid value */
		if ((endpos >= fbuflen) || (endpos == startpos))
			break;
		/*
		 * Now need to put an zero to startpos and copy
		 * everything from endpos to startpos+1
		 */
		fbuf[startpos] = '0';
		memmove(&(fbuf[startpos + 1]), &(fbuf[endpos]),
			(fbuflen - endpos));
		fbuf[(fbuflen - endpos) + (startpos + 1)] = 0;
		break;
	}
}
int32_t _update_packages_list(PRUNE_T *prune_list, int32_t num_prunes)
{
	char plistpath[METAPATHLEN];
	char plistmod[METAPATHLEN];
	FILE *src = NULL, *dst = NULL;
	int32_t ret, errcode;
	char fbuf[4100], *sptr;
	char packagename[MAX_FILENAME_LEN + 1]; /* Longest name the FS allows */
	int32_t startpos, endpos, fbuflen;
	int32_t pkgcount;

	snprintf(plistpath, METAPATHLEN, "%s/backup_pkg", RESTORE_METAPATH);
	snprintf(plistmod, METAPATHLEN, "%s/backup_pkg.mod", RESTORE_METAPATH);

	src = fopen(plistpath, "r");
	if (src == NULL) {
		errcode = -errno;
		write_log(0, "Error when opening src package list. (%s)\n",
			  strerror(-errcode));
		goto errcode_handle;
	}
	dst = fopen(plistmod, "w");
	if (dst == NULL) {
		errcode = -errno;
		write_log(0, "Error when opening dst package list. (%s)\n",
			  strerror(-errcode));
		goto errcode_handle;
	}

	clearerr(src);
	clearerr(dst);
	while (!feof(src)) {
		sptr = fgets(fbuf, 4096, src);
		if (sptr == NULL)
			break;
		fbuflen = strlen(fbuf);
		if (fbuflen < (int32_t)(5 + strlen("package name"))) {
			/* Cannot be the package info, write directly */
			fprintf(dst, "%s", fbuf);
			continue;
		}
		if (strncmp(&(fbuf[5]), "package name",
			    strlen("package name")) != 0) {
			/* Not the package info, write directly */
			fprintf(dst, "%s", fbuf);
			continue;
		}
		/* First parse the name */
		for (startpos = 5; startpos < fbuflen; startpos++)
			if (fbuf[startpos] == '"')
				break;
		if (startpos >= fbuflen) {
			/* Not the package info, write directly */
			fprintf(dst, "%s", fbuf);
			continue;
		}
		startpos++;
		for (endpos = startpos; endpos < fbuflen; endpos++)
			if (fbuf[endpos] == '"')
				break;
		if ((endpos >= fbuflen) || (endpos == startpos)) {
			/* Not the package info, write directly */
			fprintf(dst, "%s", fbuf);
			continue;
		}
		/*
		 * Limit the length of package to compare to max of
		 * folder name
		 */
		if (endpos > (startpos + MAX_FILENAME_LEN))
			endpos = startpos + MAX_FILENAME_LEN;

		strncpy(packagename, &(fbuf[startpos]), (endpos - startpos));
		packagename[endpos - startpos] = 0;

		write_log(10, "Restore processing app %s\n", packagename);

		/* Check if this package needs to be reset */
		/* If so, find the version field and replace it with zero */
		for (pkgcount = 0; pkgcount < num_prunes; pkgcount++)
			if (!strncmp(packagename,
				     prune_list[pkgcount].entry.d_name,
				     strlen(packagename))) {
				write_log(4, "Cleaning-up package %s in list",
					  packagename);
				_replace_version(fbuf, endpos, fbuflen);
				break;
			}
		fprintf(dst, "%s", fbuf);
	}
	if (ferror(src) && !feof(src)) {
		write_log(0, "Package list update terminated unexpectedly\n");
		errcode = ferror(src);
		goto errcode_handle;
	}

	fclose(src);
	src = NULL;
	fclose(dst);
	dst = NULL;
	ret = rename(plistmod, plistpath);
	if (ret < 0) {
		errcode = -errno;
		write_log(0, "Error when renaming in stage 1. (%s)\n",
			  strerror(-errcode));
		goto errcode_handle;
	}

	return 0;
errcode_handle:
	if (src != NULL)
		fclose(src);
	if (dst != NULL)
		fclose(dst);
	unlink(plistmod);
	return errcode;
}



/*
 * A test function running before main and exit at end.
 * DELETE this function when development finished!!
 */
/*void __attribute__((constructor)) dev_test(void)
{
	int32_t uid;
	const char pkg[] = "com.android.captiveportallogin";

	_init_package_uid_list();
	uid = _lookup_package_uid_list(pkg);
	printf("%s %d\n", pkg, uid);
	_destroy_package_uid_list();
	printf("\nRestore hcfs code at %s %d\n\n", __FILE__, __LINE__);
	exit(0);
}*/
/* End of package_uid_list code */

/* Function for blocking execution until network is available again */
int32_t check_network_connection(void)
{
	int32_t retries_since_last_notify = 0;

	while (hcfs_system->sync_paused == TRUE) {
		write_log(4, "Connection is not available now\n");
		write_log(4, "Sleep for 5 seconds before retrying\n");
		/* Now will notify once every 5 minutes */
		if (retries_since_last_notify >= 60) {
			notify_restoration_result(1, -ENETDOWN);
			retries_since_last_notify = 0;
		} else {
			retries_since_last_notify++;
		}
		sleep(5);
		if (hcfs_system->system_going_down == TRUE)
			return -ESHUTDOWN;
	}

	return 0;
}

int32_t read_system_max_inode(ino_t *ino_num)
{
	char despath[METAPATHLEN];
	FILE *fptr;
	int32_t errcode;
	int64_t ret_ssize;

	snprintf(despath, METAPATHLEN, "%s/system_max_inode", METAPATH);
	fptr = fopen(despath, "r");
	if (fptr == NULL) {
		write_log(0, "Error when parsing volumes to restore\n");
		return -errno;
	}
	flock(fileno(fptr), LOCK_EX);
	PREAD(fileno(fptr), ino_num, sizeof(ino_t), 0);
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	return 0;

errcode_handle:
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	return errcode;
}

int32_t write_system_max_inode(ino_t ino_num)
{
	char despath[METAPATHLEN];
	FILE *fptr;
	int32_t errcode;
	int64_t ret_ssize;

	/* Write to file */
	snprintf(despath, METAPATHLEN, "%s/system_max_inode", RESTORE_METAPATH);
	fptr = fopen(despath, "w+");
	if (fptr == NULL) {
		write_log(0, "Error when parsing volumes to restore\n");
		return -errno;
	}
	setbuf(fptr, NULL);
	flock(fileno(fptr), LOCK_EX);
	PWRITE(fileno(fptr), &ino_num, sizeof(ino_t), 0);
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	return 0;

errcode_handle:
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	return errcode;
}

int32_t run_download_minimal(void)
{
	ino_t rootino;
	char despath[METAPATHLEN];
	char restore_todelete_list[METAPATHLEN];
	char restore_tosync_list[METAPATHLEN];
	DIR_META_TYPE tmp_head;
	DIR_ENTRY_PAGE tmppage;
	FILE *fptr;
	int32_t errcode, count, ret;
	ssize_t ret_ssize;
	DIR_ENTRY *tmpentry;
	BOOL is_fopen = FALSE;
	ino_t vol_max_inode, sys_max_inode;
	INODE_PAIR_LIST *hardln_mapping;

	/* Fetch quota value from backend and store in the restoration path */

	/* First make sure that network connection is turned on */
	ret = check_network_connection();
	if (ret < 0)
		return ret;

	_init_quota_restore();
	ret = _restore_system_quota();
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	snprintf(restore_tosync_list, METAPATHLEN, "%s/tosync_list",
			RESTORE_METAPATH);
	/*
	 * FEATURE TODO: If download in stage1 can be resumed in the
	 * middle, then will need to open this list with "a+"
	 */
	to_delete_fptr = NULL;
	to_sync_fptr = fopen(restore_tosync_list, "w+");
	if (to_sync_fptr == NULL) {
		write_log(0, "Unable to open tosync list\n");
		errcode = -errno;
		goto errcode_handle;
	}

	write_log(4, "Downloading package list backup\n");
	snprintf(despath, METAPATHLEN, "%s/backup_pkg", RESTORE_METAPATH);
	ret = restore_fetch_obj("backup_pkg", despath, FALSE);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	/* Init package-uid lookup table */
	ret = init_package_uid_list(despath);
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
		goto errcode_handle;
	}
	is_fopen = TRUE;
	setbuf(fptr, NULL);
	PREAD(fileno(fptr), &tmp_head, sizeof(DIR_META_TYPE),
	      16);
	PREAD(fileno(fptr), &tmppage, sizeof(DIR_ENTRY_PAGE),
	      tmp_head.tree_walk_list_head);
	fclose(fptr);
	is_fopen = FALSE;

	/* Fetch FSstat first */
	sys_max_inode = 0;
	for (count = 0; count < tmppage.num_entries; count++) {
		tmpentry = &(tmppage.dir_entries[count]);
		if (!strcmp("hcfs_app", tmpentry->d_name) ||
		    !strcmp("hcfs_data", tmpentry->d_name) ||
		    !strcmp("hcfs_external", tmpentry->d_name)) {
			rootino = tmpentry->d_ino;
			ret = _fetch_FSstat(rootino);
			if (ret == 0)
				ret = _update_FS_stat(rootino, &vol_max_inode);
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}
			sys_max_inode = (vol_max_inode > sys_max_inode ?
					vol_max_inode : sys_max_inode);
		}
	}
	/* Write max inode number to file "system_max_inode" */
	LOCK_RESTORED_SYSMETA();
	hcfs_restored_system_meta->system_max_inode = sys_max_inode;
	UNLOCK_RESTORED_SYSMETA();
	ret = write_system_max_inode(sys_max_inode);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	/* Fetch data from root */
	for (count = 0; count < tmppage.num_entries; count++) {
		tmpentry = &(tmppage.dir_entries[count]);
		write_log(4, "Processing minimal for %s\n", tmpentry->d_name);
		if (!strcmp("hcfs_app", tmpentry->d_name)) {
			rootino = tmpentry->d_ino;
			snprintf(restore_todelete_list, METAPATHLEN,
				 "%s/todelete_list_%" PRIu64, RESTORE_METAPATH,
				 (uint64_t)rootino);
			/*
			 * FEATURE TODO: If download in stage1 can be
			 * resumed in the middle, then will need to open
			 * this list with "a+"
			 */
			if (to_delete_fptr != NULL)
				fclose(to_delete_fptr);
			to_delete_fptr = fopen(restore_todelete_list, "w+");
			if (to_delete_fptr == NULL) {
				write_log(0, "Unable to open todelete list\n");
				errcode = -errno;
				goto errcode_handle;
			}
			ret = _fetch_meta(rootino);
			if (ret == 0) {
				hardln_mapping = new_inode_pair_list();
				if (hardln_mapping == NULL) {
					errcode = -ENOMEM;
					goto errcode_handle;
				}
				ret = _expand_and_fetch(rootino,
					"/data/app", 0, hardln_mapping);
				destroy_inode_pair_list(hardln_mapping);
			}
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}
			continue;
		}
		if (!strcmp("hcfs_data", tmpentry->d_name)) {
			rootino = tmpentry->d_ino;
			snprintf(restore_todelete_list, METAPATHLEN,
				 "%s/todelete_list_%" PRIu64, RESTORE_METAPATH,
				 (uint64_t)rootino);
			/*
			 * FEATURE TODO: If download in stage1 can be
			 * resumed in the middle, then will need to open
			 * this list with "a+"
			 */
			if (to_delete_fptr != NULL)
				fclose(to_delete_fptr);
			to_delete_fptr = fopen(restore_todelete_list, "w+");
			if (to_delete_fptr == NULL) {
				write_log(0, "Unable to open todelete list\n");
				errcode = -errno;
				goto errcode_handle;
			}
			ret = _fetch_meta(rootino);
			if (ret == 0) {
				hardln_mapping = new_inode_pair_list();
				if (hardln_mapping == NULL) {
					errcode = -ENOMEM;
					goto errcode_handle;
				}
				ret = _expand_and_fetch(rootino,
					"/data/data", 0, hardln_mapping);
				destroy_inode_pair_list(hardln_mapping);
			}
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}
			continue;
		}
		if (!strcmp("hcfs_external", tmpentry->d_name)) {
			rootino = tmpentry->d_ino;
			snprintf(restore_todelete_list, METAPATHLEN,
				 "%s/todelete_list_%" PRIu64, RESTORE_METAPATH,
				 (uint64_t)rootino);
			/*
			 * FEATURE TODO: If download in stage1 can be
			 * resumed in the middle, then will need to open
			 * this list with "a+"
			 */
			if (to_delete_fptr != NULL)
				fclose(to_delete_fptr);
			to_delete_fptr = fopen(restore_todelete_list, "w+");
			if (to_delete_fptr == NULL) {
				write_log(0, "Unable to open todelete list\n");
				errcode = -errno;
				goto errcode_handle;
			}
			ret = _fetch_meta(rootino);
			if (ret == 0) {
				hardln_mapping = new_inode_pair_list();
				if (hardln_mapping == NULL) {
					errcode = -ENOMEM;
					goto errcode_handle;
				}
				ret = _expand_and_fetch(rootino,
					"/storage/emulated", 0, hardln_mapping);
				destroy_inode_pair_list(hardln_mapping);
			}
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}
			continue;
		}
	}

	destroy_package_uid_list();

	/* Write max inode number */
	ret = write_system_max_inode(
			hcfs_restored_system_meta->system_max_inode);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
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

	if (to_delete_fptr != NULL)
		fclose(to_delete_fptr);
	fclose(to_sync_fptr);
	notify_restoration_result(1, 0);

	return 0;
errcode_handle:
	if (is_fopen == TRUE)
		fclose(fptr);
	notify_restoration_result(1, errcode);
	return errcode;
}

int _delete_node(const char *thispath,
		 const struct stat *thisstat,
		 int flag,
		 struct FTW *buf)
{
	int ret, errcode = 0;

	UNUSED(buf);
	UNUSED(thisstat);
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

	snprintf(todelete_metapath, METAPATHLEN, "%s_todelete", METAPATH);
	snprintf(todelete_blockpath, BLOCKPATHLEN, "%s_todelete", BLOCKPATH);

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
 * Update restored system meta when restoring.
 *
 * @param delta_system_meta Structure of delta system space usage.
 *
 * @return none.
 */
void update_restored_system_meta(DELTA_SYSTEM_META delta_system_meta)
{
	SYSTEM_DATA_TYPE *restored_system_meta;

	restored_system_meta =
	    &(hcfs_restored_system_meta->restored_system_meta);

	LOCK_RESTORED_SYSMETA();
	/* Update restored space usage */
	restored_system_meta->system_size +=
	    delta_system_meta.delta_system_size;
	restored_system_meta->system_meta_size +=
	    delta_system_meta.delta_meta_size;
	restored_system_meta->pinned_size +=
	    delta_system_meta.delta_pinned_size;
	restored_system_meta->backend_size +=
	    delta_system_meta.delta_backend_size;
	restored_system_meta->backend_meta_size +=
	    delta_system_meta.delta_backend_meta_size;
	restored_system_meta->backend_inodes +=
	    delta_system_meta.delta_backend_inodes;
	UNLOCK_RESTORED_SYSMETA();
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
}

/**
 * Rectify the space usage in last step of restoration stage 2.
 *
 * @return 0 on success, otherwise negative errcode.
 */
int32_t rectify_space_usage(void)
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

	/*
	 * Rectify the statistics. When restoration is complete, decrease
	 * hcfs space usage statistics by error value recorded in
	 * rectified_system_meta
	 */
	LOCK_RESTORED_SYSMETA();
	change_system_meta(-rectified_system_meta->system_size,
			   -rectified_system_meta->system_meta_size, 0, 0, 0, 0,
			   TRUE);
	update_backend_usage(-rectified_system_meta->backend_size,
			     -rectified_system_meta->backend_meta_size,
			     -rectified_system_meta->backend_inodes);
	change_pin_size(-rectified_system_meta->pinned_size);
	UNLOCK_RESTORED_SYSMETA();

	/* Remove the file */
	snprintf(rectified_usage_path, METAPATHLEN,
		 "%s/hcfssystemfile.rectified", METAPATH);
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

	hcfs_restored_system_meta = (HCFS_RESTORED_SYSTEM_META *)malloc(
	    sizeof(HCFS_RESTORED_SYSTEM_META));
	memset(hcfs_restored_system_meta, 0, sizeof(HCFS_RESTORED_SYSTEM_META));
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
		/*
		 * In restoration stage 1, always create a new rectified
		 * file and re-estimate the system meta.
		 */
		if (hcfs_system->system_restoring == RESTORING_STAGE1)
			write_log(8, "Create a rectified system meta file.");
		else
			write_log(2, "%s %s",
				  "Warn: Rectified system meta file",
				  "not found in stage 2. Create an empty one.");
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
