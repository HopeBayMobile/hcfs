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
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "utils.h"
#include "macro.h"
#include "fuseop.h"
#include "event_filter.h"
#include "event_notification.h"
#include "hcfs_fromcloud.h"
#include "metaops.h"

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE

void init_restore_path(void)
{
	snprintf(RESTORE_METAPATH, METAPATHLEN, "%s_restore",
	         METAPATH);
	snprintf(RESTORE_BLOCKPATH, BLOCKPATHLEN, "%s_restore",
	         BLOCKPATH);
	sem_init(&(restore_sem), 0, 1);
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
	FILE *fptr;
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
int32_t initiate_restoration(void)
{
	int32_t ret, errcode;

	ret = check_restoration_status();
	if (ret > 0) {
		/* If restoration is already in progress, do not permit */
		return -EPERM;
	}

	sem_wait(&restore_sem);

	/* First check if cache size is enough */
	sem_wait(&(hcfs_system->access_sem));
	/* FEATURE TODO: consider
	2. current pin size
	3. current high-priority pin size
	4. current meta size (later)
	*/

	/* Need cache size to be less than 0.2 of max possible cache size */
	if (hcfs_system->systemdata.cache_size >= CACHE_HARD_LIMIT * 0.2) {
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

	retval = 0;
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
			retval = 1;
		} else if (strncmp(restore_stat, "rebuilding_meta",
		            strlen("rebuilding_meta")) == 0) {
			write_log(10, "Rebuilding meta\n");
			retval = 2;
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
		snprintf(msgstr, 100, "{\\”result\\”:%d}", result);
		ret = add_notify_event(RESTORATION_STAGE1_CALLBACK,
		                       msgstr, TRUE);
		break;
	case 2:
		/* Restoration stage 2 */
		snprintf(msgstr, 100, "{\\”result\\”:%d}", result);
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

	/* FEATURE TODO: consider
	3. current high-priority pin size
	4. current meta size (later)
	*/

	sem_wait(&(hcfs_system->access_sem));

	/* Need enough cache space */
	if ((hcfs_system->systemdata).cache_size >=
	    CACHE_HARD_LIMIT * REDUCED_RATIO) {
		sem_post(&(hcfs_system->access_sem));
		return -ENOSPC;
	}
	if ((hcfs_system->systemdata).pinned_size >=
	    MAX_PINNED_LIMIT * REDUCED_RATIO) {
		sem_post(&(hcfs_system->access_sem));
		return -ENOSPC;
	}

	CACHE_HARD_LIMIT = CACHE_HARD_LIMIT * REDUCED_RATIO;
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

int32_t restore_fetch_obj(char *objname, char *despath)
{
	FILE *fptr;
	int32_t ret;

	fptr = fopen(despath, "w");
	if (fptr == NULL) {
		write_log(0, "Unable to open file for writing\n");
		return -EIO;
	}

	ret = fetch_object_busywait_conn(fptr, RESTORE_FETCH_OBJ, objname);

	if (ret < 0)
		unlink(despath);
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

	ret = restore_fetch_obj(objname, despath);
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

	ret = restore_fetch_obj(objname, despath);
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

	ret = restore_fetch_obj(objname, despath);
	return ret;

errcode_handle:
	return errcode;
}

int32_t _fetch_pinned(ino_t thisinode)
{
	FILE_META_TYPE tmpmeta;
	struct stat tmpstat;
	FILE *fptr;
	char metapath[METAPATHLEN];
	int64_t count, totalblocks, tmpsize, seq;
	int64_t nowpage, lastpage, filepos, nowindex;
	int32_t errcode, ret;
	size_t ret_size;
	BLOCK_ENTRY_PAGE temppage;

	fetch_restore_meta_path(metapath, thisinode);
	fptr = fopen(metapath, "r");
	if (fptr == NULL) {
		write_log(0, "Error when fetching file to restore\n");
		errcode = -errno;
		return errcode;
	}

	FREAD(&tmpstat, sizeof(struct stat), 1, fptr);
	FREAD(&tmpmeta, sizeof(FILE_META_TYPE), 1, fptr);
	if (tmpmeta.local_pin == P_UNPIN) {
		/* Don't fetch blocks */
		fclose(fptr);
		return 0;
	}

	/* Assuming fixed block size now */
	tmpsize = tmpstat.st_size;
	totalblocks = ((tmpsize - 1) / 1048576) + 1;
	lastpage = -1;
	for (count = 0; count < totalblocks; count++) {
		nowpage = count / MAX_BLOCK_ENTRIES_PER_PAGE;
		nowindex = count % MAX_BLOCK_ENTRIES_PER_PAGE;
		if (lastpage != nowpage) {
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

		seq = temppage.block_entries[nowindex].seqnum;
		_fetch_block(thisinode, count, seq);
		/* FEATURE TODO: Change block status in meta */
	}

	fclose(fptr);
	return 0;
errcode_handle:
	fclose(fptr);
	return errcode;
}

int32_t _check_expand(ino_t thisinode, char *nowpath, int32_t depth)
{
	UNUSED(thisinode);

	if (strcmp(nowpath, "/data/app") == 0)
		return 1;

	if (strcmp(nowpath, "/data/data") == 0)
		return 1;

	/* Expand only /storage/emulated/Android */
	if (strcmp(nowpath, "/storage/emulated") == 0)
		return 2;

	/* If not high-priority pin, in /data/data only keep the lib symlinks */
	if ((strncmp(nowpath, "/data/data", strlen("/data/data")) == 0) &&
	    (depth == 1))
		return 3;

	if ((strncmp(nowpath, "/storage/emulated", strlen("/storage/emulated")) == 0) &&
	    ((depth == 1) || (depth == 2)))
		return 1;

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

	FSEEK(fptr, sizeof(struct stat), SEEK_SET);
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

			/* First fetch the meta */
			tmpino = tmpptr->d_ino;
			ret = _fetch_meta(tmpino);
			if (ret < 0) {
				/* FEATURE TODO: error handling,
				such as shutdown */
				continue;
			}

			switch (tmpptr->d_type) {
			case D_ISLNK:
				/* Just fetch the meta */
				break;
			case D_ISREG:
			case D_ISFIFO:
			case D_ISSOCK:
				/* Fetch all blocks if pinned */
				_fetch_pinned(tmpino);
				break;
			case D_ISDIR:
				/* Need to expand */
				snprintf(tmppath, METAPATHLEN, "%s/%s",
				         nowpath, tmpptr->d_name);
				_expand_and_fetch(tmpino, tmppath, depth + 1);
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

/* FEATURE TODO: Verify if downloaded objects are enough for restoration */
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

	snprintf(despath, METAPATHLEN, "%s/fsmgr", RESTORE_METAPATH);
	ret = restore_fetch_obj("FSmgr_backup", despath);

	if (ret < 0)
		return ret;

	/* Parse file mgr */
	fptr = fopen(despath, "r");
	if (fptr == NULL) {
		write_log(0, "Error when parsing volumes to restore\n");
		errcode = -errno;
		return errcode;
	}

	setbuf(fptr, NULL);

	PREAD(fileno(fptr), &tmp_head, sizeof(DIR_META_TYPE),
	      16);
	PREAD(fileno(fptr), &tmppage, sizeof(DIR_ENTRY_PAGE),
	      tmp_head.tree_walk_list_head);

	for (count = 0; count < tmppage.num_entries; count++) {
		tmpentry = &(tmppage.dir_entries[count]);
		write_log(4, "Processing minimal for %s\n",
		          tmpentry->d_name);
		if (!strcmp("hcfs_app", tmpentry->d_name)) {
			rootino = tmpentry->d_ino;
			_fetch_meta(rootino);
			_fetch_FSstat(rootino);
			_expand_and_fetch(rootino, "/data/app", 0);
			continue;
		}
		if (!strcmp("hcfs_data", tmpentry->d_name)) {
			rootino = tmpentry->d_ino;
			_fetch_meta(rootino);
			_fetch_FSstat(rootino);
			_expand_and_fetch(rootino, "/data/data", 0);
		}
		if (!strcmp("hcfs_external", tmpentry->d_name)) {
			rootino = tmpentry->d_ino;
			_fetch_meta(rootino);
			_fetch_FSstat(rootino);
			_expand_and_fetch(rootino, "/storage/emulated", 0);
		}
	}

	sem_wait(&restore_sem);

	/* Tag status of restoration */
	ret = tag_restoration("rebuilding_meta");
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	sem_post(&restore_sem);

	notify_restoration_result(1, 0);

	return 0;
errcode_handle:
	fclose(fptr);
	notify_restoration_result(1, errcode);
	return errcode;
}
