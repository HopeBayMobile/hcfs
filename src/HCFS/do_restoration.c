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
	1. current cache size
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

void start_download_minimal(void)
{
/* FEATURE TODO:
1. Setup path for downloading minimal
2. Fork detachable threads
3. Return
*/
	return;
}
