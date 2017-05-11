/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: syncpoint_control.c
* Abstract: The c source code file for setting sync point.
*
* Revision History
* 2016/7/12 Kewei created this file.
*
**************************************************************************/

#include "syncpoint_control.h"

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#include "global.h"
#include "params.h"
#include "logger.h"
#include "macro.h"
#include "super_block.h"
#include "event_notification.h"
#include "event_filter.h"
#include "hcfs_fromcloud.h"
#include "fuseop.h"

/**
 * Fetch path of sync point data.
 */
void fetch_syncpoint_data_path(char *path)
{
	sprintf(path, "%s/sync_point_data", METAPATH);
}

/**
 * Allocate the necessary memory and file when proccessing to sync all data.
 * If sync point data file exists, just open it and load data to memory.
 * Otherwise create a file and init all data to be zero.
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t init_syncpoint_resource()
{
	char path[METAPATHLEN];
	FILE *temp_fptr;
	int32_t errcode;
	int64_t ret_ssize;

	if (!sys_super_block) {
		write_log(0, "Error: Superblock should be initialized"
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

	/* Check if sync is paused, and wake it up if needed */
	int32_t sem_val = 0;
	sem_getvalue(&(hcfs_system->sync_control_sem), &sem_val);
	if (sem_val <= 0)
		sem_post(&(hcfs_system->sync_control_sem));

	memset(sys_super_block->sync_point_info, 0, sizeof(SYNC_POINT_INFO));
	sys_super_block->sync_point_info->sync_retry_times = SYNC_RETRY_TIMES;
	sem_init(&(sys_super_block->sync_point_info->ctl_sem), 0, 1);
	fetch_syncpoint_data_path(path);

	if (!access(path, F_OK)) {
		temp_fptr = fopen(path, "r+");
		if (!temp_fptr) {
			FREE(sys_super_block->sync_point_info);
			return -errno;
		}
		sys_super_block->sync_point_info->fptr = temp_fptr;
		PREAD(fileno(temp_fptr),
				&(sys_super_block->sync_point_info->data),
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

/**
 * Release all memory resource and remove sync point data file. Also
 * reset the flag "sync_point_is_set".
 *
 * @param remove_file Flag indicates whether data file should be removed.
 *
 * @return none.
 */
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

/**
 * Flush sync point data to file.
 *
 * @return 0 on success, otherwise negative error code.
 */
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

void sync_complete_send_event()
{
	int32_t ret;

	/* Block until event had been added into queue. */
	ret = add_notify_event(SYNCDATACOMPLETE, NULL, TRUE);
	if (ret < 0) {
		write_log(0, "Error: Fail to add event in %s."
				" Code %d\n", __func__, -ret);
	} else if (ret > 0) {
		write_log(0, "Warn: Give up to add event in %s."
				" Code %d\n", __func__, ret);
	}
	return;
}

static inline void _reset_upload_sync_point()
{
	SYNC_POINT_DATA *data;

	data = &(sys_super_block->sync_point_info->data);
	data->upload_sync_point =
		sys_super_block->head.last_dirty_inode;
	data->upload_sync_complete = FALSE;
}

static inline void _reset_delete_sync_point()
{
	SYNC_POINT_DATA *data;

	data = &(sys_super_block->sync_point_info->data);
	data->delete_sync_point =
		sys_super_block->head.last_to_delete_inode;
	data->delete_sync_complete = FALSE;
}
/**
 * Check if both upload and delete thread all complete. If both of them complete
 * to sync all data, then check whether there are dirty data in queue. If it is
 * still dirty, then reset sync point and sync again.
 *
 * @return none.
 */
void check_sync_complete(char which_ll, ino_t this_inode)
{
	SYNC_POINT_DATA *data;
	BOOL sync_complete;
	pthread_t event_thread;
	int32_t ret;

	data = &(sys_super_block->sync_point_info->data);

	/* Return if system is still dirty. */
	if (!(data->upload_sync_complete && data->delete_sync_complete))
		return;

	sync_complete = TRUE;
	/* Retry some times */
	if (sys_super_block->sync_point_info->sync_retry_times > 0) {
		/* Reset the sync point if queue is not empty. */
		switch (which_ll) {
		case IS_DIRTY:
			if (sys_super_block->head.last_dirty_inode !=
					this_inode) {
				_reset_upload_sync_point();
				sync_complete = FALSE;
			}
			if (sys_super_block->head.last_to_delete_inode != 0) {
				_reset_delete_sync_point();
				sync_complete = FALSE;
			}
			break;
		case TO_BE_DELETED:
			if (sys_super_block->head.last_dirty_inode != 0) {
				_reset_upload_sync_point();
				sync_complete = FALSE;
			}
			if (sys_super_block->head.last_to_delete_inode !=
					this_inode) {
				_reset_delete_sync_point();
				sync_complete = FALSE;
			}
			break;
		}
		sys_super_block->sync_point_info->sync_retry_times -= 1;
		if (sync_complete == FALSE) {
			write_syncpoint_data();
			write_log(6, "Info: System is still dirty,"
					" sync again\n");
		}
	}

	/* Check if task complete or not */
	if (sync_complete) {
		write_log(10, "Debug: Sync all data completed.\n");
		free_syncpoint_resource(TRUE);

		/* Send event to UI so as to tell that sync completed */
		ret = add_notify_event(SYNCDATACOMPLETE, NULL, FALSE);
		if (ret < 0) {
			write_log(0, "Error: Fail to add event in %s."
					" Code %d\n", __func__, -ret);
		} else if (ret > 0) {
			write_log(4, "Warn: Retry to send "
					"sync_complete event\n");
			ret = pthread_create(&event_thread,
					&prefetch_thread_attr,
					(void *)&sync_complete_send_event,
					NULL);
			if (ret != 0) {
				write_log(0, "Error: Fail to create"
						" thread in %s. Code %d\n",
						__func__, ret);
			}
		}
	}

	return;
}

/**
 * Move sync point when sync point is going to be dequeued from dirty queue
 * or delete queue. After moving sync point, check if sync completes.
 *
 * @param which_ll Original status of "this_inode"
 * @param this_inode Inode to be dequeued
 * @param this_entry Super block entry to be dequeued
 *
 * @return none
 */
void move_sync_point(char which_ll, ino_t this_inode,
			SUPER_BLOCK_ENTRY *this_entry)
{
	SYNC_POINT_DATA *syncpoint_data;
	BOOL need_write;

	if (!(sys_super_block->sync_point_info))
		return;

	need_write = FALSE;
	syncpoint_data = &(sys_super_block->sync_point_info->data);
	switch (which_ll) {
	case IS_DIRTY:
		/* Check when upload thread does not complete. */
		if (syncpoint_data->upload_sync_complete == FALSE) {
			if (syncpoint_data->upload_sync_point == this_inode) {
				syncpoint_data->upload_sync_point =
						this_entry->util_ll_prev;
				need_write = TRUE;
			}
			if (syncpoint_data->upload_sync_point == 0) {
				syncpoint_data->upload_sync_complete = TRUE;
				need_write = TRUE;
			}
		}
		if (need_write) {
			write_syncpoint_data();
			check_sync_complete(which_ll, this_inode);
		}
		break;
	case TO_BE_DELETED:
		/* Check when delete thread does not complete. */
		if (syncpoint_data->delete_sync_complete == FALSE)  {
			if (syncpoint_data->delete_sync_point == this_inode) {
				syncpoint_data->delete_sync_point =
						this_entry->util_ll_prev;
				need_write = TRUE;
			}
			if (syncpoint_data->delete_sync_point == 0) {
				syncpoint_data->delete_sync_complete = TRUE;
				need_write = TRUE;
			}
		}
		if (need_write) {
			write_syncpoint_data();
			check_sync_complete(which_ll, this_inode);
		}
		break;
	default:
		break;
	}

	return;
}
