/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: pin_scheduling.c
* Abstract: The c source code file for checking files in pin-queue and
*           scheduling to pin files.
*
* Revision History
* 2015/11/6 Kewei created this file.
* 2015/12/15 Kewei added multi-threads policy in pinning_loop().
*
**************************************************************************/

#include "pin_scheduling.h"

#include <pthread.h>
#include <inttypes.h>

#include "super_block.h"
#include "hcfs_fromcloud.h"
#include "global.h"
#include "errno.h"
#include "logger.h"
#include "utils.h"

int32_t init_pin_scheduler()
{
	memset(&pinning_scheduler, 0, sizeof(PINNING_SCHEDULER));
	sem_init(&(pinning_scheduler.ctl_op_sem), 0, 1);
	sem_init(&(pinning_scheduler.pinning_sem), 0,
			MAX_PINNING_FILE_CONCURRENCY);

	pthread_create(&pinning_scheduler.pinning_collector, NULL,
			(void *)pinning_collect, NULL);
	pthread_create(&pinning_scheduler.pinning_manager, NULL,
			(void *)pinning_loop, NULL);
	write_log(10, "Debug: Create pinning scheduler\n");

	return 0;
}

int32_t destroy_pin_scheduler()
{
	pthread_join(pinning_scheduler.pinning_manager, NULL);
	pthread_join(pinning_scheduler.pinning_collector, NULL);
	sem_destroy(&(pinning_scheduler.ctl_op_sem));
	sem_destroy(&(pinning_scheduler.pinning_sem));
	write_log(10, "Debug: Pinning scheduler thread terminated\n");

	return 0;
}

static BOOL _pinning_wakeup_fn()
{
	return hcfs_system->system_going_down;
}

void _sleep_a_while(uint32_t *rest_times)
{
	uint32_t level;

	level = (*rest_times) / 60;

	if (level < 5) {
		nonblock_sleep(level + 1, _pinning_wakeup_fn);
		*rest_times += 1;
	} else {
		nonblock_sleep(5, _pinning_wakeup_fn);
	}

	return;
}

/**
 * pinning_collect
 *
 * This is a thread collector for the purpose of joinning those pinning
 * threads that finished their works.
 *
 * @param None
 *
 * @return None
 */
void pinning_collect()
{
	int32_t idx;
	struct timespec time_to_sleep;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	while (TRUE) {
		/* Wait for threads */
		if (hcfs_system->system_going_down == TRUE) {
			if (pinning_scheduler.total_active_pinning <= 0) {
				break;
			}
		}

		if (pinning_scheduler.total_active_pinning <= 0) {
			sleep(1);
			continue;
		}

		/* Collect threads */
		sem_wait(&(pinning_scheduler.ctl_op_sem));
		for (idx = 0; idx < MAX_PINNING_FILE_CONCURRENCY; idx++) {
			/* Join it when thread is active and finished */
			if (pinning_scheduler.thread_active[idx] == FALSE ||
				pinning_scheduler.thread_finish[idx] == FALSE)
				continue;
			sem_post(&(pinning_scheduler.ctl_op_sem));
			pthread_join(pinning_scheduler.pinning_file_tid[idx],
					NULL);
			sem_wait(&(pinning_scheduler.ctl_op_sem));

			/* Recycle this thread */
			pinning_scheduler.thread_active[idx] = FALSE;
			pinning_scheduler.thread_finish[idx] = FALSE;
			memset(&(pinning_scheduler.pinning_info[idx]), 0,
				sizeof(PINNING_INFO));
			pinning_scheduler.total_active_pinning--;

			/* Post a semaphore */
			sem_post(&(pinning_scheduler.pinning_sem));

		}
		sem_post(&(pinning_scheduler.ctl_op_sem));
		nanosleep(&time_to_sleep, NULL);
	}
}

/**
 * pinning_worker
 *
 * This function aims to call "fetch_pinned_blocks()" and appropriately handle
 * the error when failing. If succeeding in fetch all blocks, then call
 * "super_block_finish_pinning()" so that dequeue the inode from pinning queue.
 *
 * @param ptr A pointer points to needed data when pinning
 *
 * @return None
 */
void pinning_worker(void *ptr)
{
	PINNING_INFO *pinning_info;
	ino_t this_inode;
	int32_t t_idx, ret;

	pinning_info = (PINNING_INFO *)ptr;
	this_inode = pinning_info->this_inode;
	t_idx = pinning_info->t_idx;

	ret = fetch_pinned_blocks(this_inode);
	if (ret < 0) {
		if (ret == -ENOSPC) { /* Retry it later */
			write_log(4, "Warn: No space available to pin"
					" inode %"PRIu64"\n",
					(uint64_t)this_inode);

			sem_wait(&(pinning_scheduler.ctl_op_sem));
			pinning_scheduler.deep_sleep = TRUE;
			sem_post(&(pinning_scheduler.ctl_op_sem));

		} else if (ret == -EIO) { /* Retry it later */
			write_log(0, "Error: IO error when pinning"
					" inode %"PRIu64". Try it later\n",
					(uint64_t)this_inode);

		} else if (ret == -ENOENT) { /* It is dequeued and deleted */
			/* It was deleted and dequeued */
			write_log(10, "Debug: Inode %"PRIu64" is "
					"deleted when pinning\n",
					(uint64_t)this_inode);
		}
		pinning_scheduler.thread_finish[t_idx] = TRUE;
		return; /* Do not dequeue when failing in pinning */
	}

	ret = super_block_finish_pinning(this_inode);
	if (ret < 0) {
		write_log(0, "Error: Fail to mark inode %"PRIu64" as"
				" finishing pinning. Code %d\n",
				(uint64_t)this_inode, -ret);
	} else {
		write_log(10, "Debug: Succeeding in marking "
				"inode %"PRIu64" as finishing pinning.\n",
				(uint64_t)this_inode);
	}

	pinning_scheduler.thread_finish[t_idx] = TRUE;
	return;
}

/**
 * pinning_loop
 *
 * This main loop is in purpose to poll the pinning queue and create threads
 * to pin all of those inodes when they are in this queue.
 *
 * @param None
 *
 * @return None
 */
void pinning_loop()
{
	ino_t now_inode;
	uint32_t rest_times;
	int32_t ret, i, t_idx;
	char found, start_from_head;
	SUPER_BLOCK_ENTRY sb_entry;

	rest_times = 0;
	start_from_head = TRUE;

	/**** Begin to find some inodes and pin them ****/
	while (hcfs_system->system_going_down == FALSE) {
		/* Check backend status */
		if (hcfs_system->sync_paused) {
			write_log(10, "Debug: sync paused. pinning"
				" manager takes a break\n");
			_sleep_a_while(&rest_times);
			continue;
		}

		/* Check inodes in queue */
		if (sys_super_block->head.num_pinning_inodes == 0) {
			write_log(10, "Debug: pinning manager takes a break\n");
			_sleep_a_while(&rest_times);
			continue;
		}

		/* Deeply sleeping is caused by thread error (ENOSPC) */
		if (pinning_scheduler.deep_sleep == TRUE) {
			nonblock_sleep(5, _pinning_wakeup_fn);/* Sleep 5 secs */
			sem_wait(&(pinning_scheduler.ctl_op_sem));
			pinning_scheduler.deep_sleep = FALSE;
			sem_post(&(pinning_scheduler.ctl_op_sem));
		}

		if (start_from_head == TRUE) {
			/* Sleep 1 sec if inodes are now being handled */
			super_block_share_locking();
			if (sys_super_block->head.num_pinning_inodes <=
				pinning_scheduler.total_active_pinning) {
				super_block_share_release();
				sleep(1);
				continue;
			}
			now_inode = sys_super_block->head.first_pin_inode;
			super_block_share_release();
			start_from_head = FALSE;
		}

		/* Get a semaphore and begin to pin now_inode */
		sem_wait(&(pinning_scheduler.pinning_sem));
		if (hcfs_system->system_going_down == TRUE) {
			sem_post(&(pinning_scheduler.pinning_sem));
			break;
		}
		/* Check whether this inode is now being handled */
		while (now_inode) {
			found = FALSE;
			sem_wait(&(pinning_scheduler.ctl_op_sem));
			for (i = 0; i < MAX_PINNING_FILE_CONCURRENCY; i++) {
				if (pinning_scheduler.pinning_info[i].this_inode
					== now_inode) {
					found = TRUE;
					break;
				}
			}
			sem_post(&(pinning_scheduler.ctl_op_sem));
			super_block_share_locking();
			ret = super_block_read(now_inode, &sb_entry);
			super_block_share_release();
			if (ret < 0) {
				write_log(0, "Error: Fail to read sb "
						"entry. Code %d\n", -ret);
				now_inode = 0;
				break;
			}

			if (found == TRUE) { /* Pin next file */
				now_inode = sb_entry.pin_ll_next;
			} else {
				if (sb_entry.pin_status == ST_PIN) {
					write_log(4, "Warn: inode %"PRIu64
						" in pin queue with status"
						" STPIN\n", (uint64_t)now_inode);
					now_inode = sb_entry.pin_ll_next;
					continue;
				}
				/* This file may be removed, so just
				 * start from head of queue */
				if (sb_entry.pin_status != ST_PINNING)
					now_inode = 0;
				break;
			}
		}
		if (now_inode == 0) { /* End of pinning queue */
			sem_post(&(pinning_scheduler.pinning_sem));
			start_from_head = TRUE;
			continue;
		}

		/* Begin to pin this inode */
		write_log(10, "Debug: Begin to pin inode %"PRIu64"\n",
			(uint64_t)now_inode);
		rest_times = 0;

		sem_wait(&(pinning_scheduler.ctl_op_sem));
		for (i = 0; i < MAX_PINNING_FILE_CONCURRENCY; i++) {
			if (pinning_scheduler.thread_active[i] == FALSE) {
				t_idx = i;
				break;
			}
		}
		pinning_scheduler.pinning_info[t_idx].this_inode = now_inode;
		pinning_scheduler.pinning_info[t_idx].t_idx = t_idx;
		pinning_scheduler.thread_active[t_idx] = TRUE;
		pinning_scheduler.thread_finish[t_idx] = FALSE;
		pinning_scheduler.total_active_pinning++;
		pthread_create(&pinning_scheduler.pinning_file_tid[t_idx], NULL,
				(void *)&pinning_worker,
				(void *)&pinning_scheduler.pinning_info[t_idx]);
		sem_post(&(pinning_scheduler.ctl_op_sem));

		/* Find next inode to be pinned */
		super_block_share_locking();
		ret = super_block_read(now_inode, &sb_entry);
		super_block_share_release();
		if (ret < 0) {
			write_log(0, "Error: Fail to read sb "
					"entry. Code %d\n", -ret);
			now_inode = 0;
		} else {
			now_inode = sb_entry.pin_ll_next;
		}
	}
	/**** End of while loop ****/

	write_log(10, "Debug: Leave pin_loop and system is going down\n");
}
