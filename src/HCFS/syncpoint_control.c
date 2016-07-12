/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: super_block.c
* Abstract: The c source code file for setting sync point.
*
* Revision History
* 2016/7/12 Kewei created this file.
*
**************************************************************************/

#include "syncpoint_control.h"

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#include "global.h"
#include "params.h"
#include "logger.h"
#include "macro.h"
#include "super_block.h"

void fetch_syncpoint_data_path(char *path)
{
	sprintf(path, "%s/sync_point_data", METAPATH);
}

int32_t init_syncpoint_resource()
{
	char path[METAPATHLEN];
	FILE *temp_fptr;
	int32_t errcode;
	int64_t ret_ssize;

	if (!sys_super_block) {
		write_log(0, "Error: Superblock should be initializedi"
			" before init sync point data.");
		return -EINVAL;
	}

	/* If resource had been allocated, first free them. */
	if (sys_super_block->sync_point_info) {
		write_log(4, "Warn: Sync point resource is not freed\n");
		if (sys_super_block->sync_point_info->fptr)
			fclose(sys_super_block->sync_point_info->fptr);
		sem_destroy(&(sys_super_block->sync_point_info->ctl_sem));
		FREE(sys_super_block->sync_point_info);	
	}

	/* Begin to allocate memory and open file */
	sys_super_block->sync_point_is_set = TRUE;
	sys_super_block->sync_point_info = (SYNC_POINT_INFO *)
		malloc(sizeof(SYNC_POINT_INFO));
	if (!(sys_super_block->sync_point_info)) {
		FREE(sys_super_block->sync_point_info);	
		return -ENOMEM;
	}
	memset(sys_super_block->sync_point_info, 0,
			sizeof(SYNC_POINT_INFO));

	sem_init(&(sys_super_block->sync_point_info->ctl_sem), 0, 1);
	fetch_syncpoint_data_path(path);
	
	if (!access(path, F_OK)) {
		temp_fptr = fopen(path, "r+");
		if (!temp_fptr) {
			FREE(sys_super_block->sync_point_info);	
			return -errno;
		}
		sys_super_block->sync_point_info->fptr = temp_fptr;
		PREAD(fileno(temp_fptr), &(sys_super_block->sync_point_info->data),
				sizeof(SYNC_POINT_DATA), 0);
		return 0;
	}

	/* Open file */
	temp_fptr = fopen(path, "w+");
	if (!temp_fptr) {
		FREE(sys_super_block->sync_point_info);	
		return -errno;
	}
	sys_super_block->sync_point_info->fptr = temp_fptr;
	PWRITE(fileno(temp_fptr), &(sys_super_block->sync_point_info->data),
			sizeof(SYNC_POINT_DATA), 0);

	return 0;

errcode_handle:
	free_syncpoint_resource(TRUE);
	return errcode;
}

void free_syncpoint_resource(BOOL remove_file)
{
	char path[METAPATHLEN];

	if (!(sys_super_block->sync_point_info))
		return;

	if (sys_super_block->sync_point_info->fptr) {
		fclose(sys_super_block->sync_point_info->fptr);
		if (remove_file == TRUE) {
			fetch_syncpoint_data_path(path);
			unlink(path);
		}
	}
	sem_destroy(&(sys_super_block->sync_point_info->ctl_sem));
	FREE(sys_super_block->sync_point_info);

	sys_super_block->sync_point_is_set = FALSE;
	write_log(10, "Debug: syncpoint resource was freed.\n");
	return;
}

int32_t write_syncpoint_data()
{
	int32_t errcode;
	int64_t ret_ssize;

	sem_wait(&(sys_super_block->sync_point_info->ctl_sem));
	PWRITE(fileno(sys_super_block->sync_point_info->fptr),
			&(sys_super_block->sync_point_info->data),
			sizeof(SYNC_POINT_DATA), 0);
	sem_post(&(sys_super_block->sync_point_info->ctl_sem));

errcode_handle:
	return errcode;
}

void check_sync_complete()
{
	SYNC_POINT_DATA *data;

	data = &(sys_super_block->sync_point_info->data);
	if (data->upload_sync_complete && data->delete_sync_complete) {
		write_log(10, "Debug: Sync all data completed.\n");
		free_syncpoint_resource(TRUE);
	}

	return;
}

void move_sync_point(char which_ll, ino_t this_inode,
			SUPER_BLOCK_ENTRY *this_entry)
{
	SYNC_POINT_DATA *syncpoint_data;

	if (!(sys_super_block->sync_point_info))
		return;

	syncpoint_data = &(sys_super_block->sync_point_info->data);
	switch (which_ll) {
	case IS_DIRTY:
			/* Check when upload thread does not complete. */
		if (syncpoint_data->upload_sync_complete == FALSE) {
			if (syncpoint_data->upload_sync_point == this_inode)
				syncpoint_data->upload_sync_point =
						this_entry->util_ll_prev;
			if (syncpoint_data->upload_sync_point == 0)
				syncpoint_data->upload_sync_complete = TRUE;
		}
		write_syncpoint_data();
		check_sync_complete();
		break;
	case TO_BE_DELETED:
		/* Check when delete thread does not complete. */
		if (syncpoint_data->delete_sync_complete == FALSE)  {
			if (syncpoint_data->delete_sync_point == this_inode)
				syncpoint_data->delete_sync_point =
						this_entry->util_ll_prev;
			if (syncpoint_data->delete_sync_point == 0)
				syncpoint_data->delete_sync_complete = TRUE;
		}
		write_syncpoint_data();
		check_sync_complete();
		break;
	default:
		break;
	}

	return;
}
