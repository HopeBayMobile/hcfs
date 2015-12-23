/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: pin_scheduling.c
* Abstract: The c source code file for checking files in pin-queue and
*           scheduling to pin files.
*
* Revision History
* 2015/11/6 Kewei create this file.
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

int init_pin_scheduler()
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

int destroy_pin_scheduler()
{
	pthread_join(pinning_scheduler.pinning_manager, NULL);
	pthread_join(pinning_scheduler.pinning_collector, NULL);
	sem_destroy(&(pinning_scheduler.ctl_op_sem));
	sem_destroy(&(pinning_scheduler.pinning_sem));
	write_log(10, "Debug: Pinning scheduler thread terminated\n");

	return 0;
}

void _sleep_a_while(long long rest_times)
{
	long long level;

	level = rest_times / 60;
	if (level < 5)
		sleep(level+1);
	else
		sleep(5);
}

void pinning_collector()
{
	int idx;
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
			if (pinning_scheduler.pinning_inodes[idx] == 0)
				continue;
			sem_post(&(pinning_scheduler.ctl_op_sem));
			pthread_join(pinning_scheduler.pinning_file_tid[idx],
					NULL);
			sem_wait(&(pinning_scheduler.ctl_op_sem));
			pinning_scheduler.pinning_inodes = 0;
			pinning_scheduler.total_active_pinning--;

			/* Post a semaphore */
			sem_post(&(pinning_scheduler.pinning_sem));

		}
		sem_post(&(pinning_scheduler.ctl_op_sem));
		nanosleep(&time_to_sleep, NULL);
	}
}

void pinning_worker(void *ptr)
{
	ino_t this_inode;

	this_inode = *(ino_t *)ptr;
	ret = fetch_pinned_blocks(this_inode);
	if (ret < 0) {
		if (ret == -ENOSPC) { /* Retry it later */
			write_log(4, "Warn: No space available to pin"
					" inode %"PRIu64"\n",
					(uint64_t)this_inode);
			//sleep(5);
		} else if (ret == -EIO) { /* Retry it later */
			write_log(0, "Error: IO error when pinning"
					" inode %"PRIu64". Try it later\n",
					(uint64_t)this_inode);
			//sleep(5);
		} else if (ret == -ENOENT) {
			/* It was deleted and dequeued */
			write_log(10, "Debug: Inode %"PRIu64" is "
					"deleted when pinning\n",
					(uint64_t)this_inode);
		} else if (ret == -ENOTCONN) {
			// TODO: error handling in case of disconnection
		} else if (ret == -ESHUTDOWN) {
			/* When system going down, do not mark finish
			   and directly leave the loop */
			return;
		}
		return; /* Do not dequeue when failing in pinning */
	}

	ret = super_block_finish_pinning(this_inode);
	if (ret < 0) {
		write_log(0, "Error: Fail to mark inode%"PRIu64" as"
				" finishing pinning. Code %d\n",
				(uint64_t)this_inode, -ret);
	} else {
		write_log(10, "Debug: Succeeding in marking "
				"inode %"PRIu64" as finishing pinning.\n",
				(uint64_t)this_inode);
	}

}

void pinning_loop()
{
	ino_t now_inode;
	long long rest_times;
	int ret, i, t_idx;
	char found, start_pinning;
	SUPER_BLOCK_ENTRY sb_entry;

	rest_times = 0;
	start_pinning = TRUE;
	/**** Begin to find some inodes and pin them ****/
	while (hcfs_system->system_going_down == FALSE) {
		/* Check backend status */
		if (hcfs_system->sync_paused) {
			write_log(10, "Debug: sync paused. pinning"
				" manager takes a break\n");
			_sleep_a_while(rest_times);
			rest_times++;
			continue;
		}

		/* Check inodes in queue */
		if (sys_super_block->head.num_pinning_inodes == 0) {
			write_log(10, "Debug: pinning manager takes a break\n");
			_sleep_a_while(rest_times);
			rest_times++;
			continue;
		}

		if (start_pinning == TRUE) {
			if (sys_super_block->head.num_pinning_inodes <=
				pinning_scheduler.total_active_pinning) {
				sleep(1);
				continue;
			}
			now_inode = sys_super_block->head.first_pin_inode;
			start_pinning = FALSE;
		}

		/* Check whether this inode is handled */
		sem_wait(&(pinning_scheduler.pinning_sem));
		while (now_inode) {
			found = FALSE;
			sem_wait(&(pinning_scheduler.ctl_op_sem));
			for (i = 0; i < MAX_PINNING_FILE_CONCURRENCY; i++) {
				if (pinning_scheduler.pinning_inodes[i] ==
						now_inode) {
					found = TRUE;
					break;
				}
			}
			sem_post(&(pinning_scheduler.ctl_op_sem));
			if (found == TRUE) { /* Pin next file */
				super_block_read(now_inode, &sb_entry);
				now_inode = sb_entry.pin_ll_next;
				if (now_inode == 0) /* Start from head */
					start_pinning = TRUE;
			} else {
				break;
			}
		}
		if (start_pinning == TRUE) {
			sem_post(&(pinning_scheduler.pinning_sem));
			continue;
		}

		/* Begin to pin this inode */
		write_log(10, "Debug: Begin to pin inode %"PRIu64"\n",
			(uint64_t)now_inode);
		rest_times = 0;
	
		sem_wait(&(pinning_scheduler.ctl_op_sem));
		for (i = 0; i < MAX_PINNING_FILE_CONCURRENCY; i++) {
			if (pinning_scheduler.pinning_inodes[i] == 0) {
				t_idx = i;
				break;
			}
		}
		pthread_create(&pinning_scheduler.pinning_file_tid[t_idx], NULL,
				(void *)&pinning_worker, NULL);
		sem_post(&(pinning_scheduler.ctl_op_sem));
	}
	/**** End of while loop ****/

	write_log(10, "Debug: Leave pin_loop and system is going down\n");
}
