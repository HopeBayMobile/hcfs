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

#include "super_block.h"
#include "hcfs_fromcloud.h"
#include "global.h"
#include "errno.h"
#include "logger.h"

int init_pin_scheduler()
{
	pthread_create(&pinning_scheduler.pinning_manager, NULL,
		(void *)pinning_loop, NULL);
	write_log(10, "Debug: Create pinning scheduler\n");

	return 0;
}

int destroy_pin_scheduler()
{
	pthread_join(pinning_scheduler.pinning_manager, NULL);
	write_log(10, "Debug: Terminate pinning scheduler thread\n");

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

void pinning_loop()
{
	ino_t now_inode;
	long long rest_times;
	int ret;

	rest_times = 0;
	while (hcfs_system->system_going_down == FALSE) {
		super_block_share_locking();
		if (sys_super_block->head.num_pinning_inodes == 0) {
			super_block_share_release();
			_sleep_a_while(rest_times);
			write_log(10, "Debug: pinning manager takes a break\n");
			rest_times++;
			continue;
		}
		
		now_inode = sys_super_block->head.first_pin_inode;
		super_block_share_release();
		
		rest_times = 0;
		ret = fetch_pinned_blocks(now_inode);
		if (ret < 0) {
			if (ret == -ENOSPC) { /* Retry it later */
				write_log(4, "Warn: No space available to pin"
					" inode %"FMT_INO_T"\n", now_inode);
				sleep(5);
			} else if (ret == -EIO) { /* Retry it later */
				write_log(0, "Error: IO error when pinning"
					" inode %"FMT_INO_T". Try it later\n",
					now_inode);
				sleep(5);
			} else if (ret == -ENOENT) { 
				/* It was deleted and dequeued */
				write_log(10, "Debug: Inode %"FMT_INO_T" is "
					"deleted when pinning\n", now_inode);
			} else if (ret == -ENOTCONN) {
				// TODO: error handling on case of disconnection
			}
			continue; /* Do not dequeue when failing in pinning */
		}
		
		ret = super_block_finish_pinning(now_inode);
		if (ret < 0) {
			write_log(0, "Error: Fail to mark inode%"FMT_INO_T" as"
				" finishing pinning. Code %d\n",
				now_inode, -ret);
		}
	}

	write_log(10, "Debug: Leave pin_loop and system is going down\n");
}
