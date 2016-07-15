/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: tocloud_tools.h
* Abstract: The c header file corresponding to tocloud_tools.c.
*
* Revision History
* 2016/2/15 Kewei create this file
*
**************************************************************************/
#ifndef GW20_HCFS_HCFS_TOCLOUD_TOOLS_H_
#define GW20_HCFS_HCFS_TOCLOUD_TOOLS_H_

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/types.h>
#include <inttypes.h>

typedef struct IMMEDIATELY_RETRY_LIST {
	int32_t num_retry;
	int32_t list_size;
	ino_t *retry_inode;
} IMMEDIATELY_RETRY_LIST; 

int32_t change_block_status_to_BOTH(ino_t inode, int64_t blockno,
		int64_t page_pos, int64_t toupload_seq);

int delete_backend_blocks(int progress_fd, long long total_blocks, ino_t inode,
		char delete_which_one);

void busy_wait_all_specified_upload_threads(ino_t inode);

ino_t pull_retry_inode(IMMEDIATELY_RETRY_LIST *list);
void push_retry_inode(IMMEDIATELY_RETRY_LIST *list, ino_t inode);

#endif
