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
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/types.h>
#include <inttypes.h>


int change_block_status_to_BOTH(ino_t inode, long long blockno,
		long long page_pos, long long toupload_seq);

int change_status_to_BOTH(ino_t inode, int progress_fd,
		FILE *local_metafptr, char *local_metapath);

int delete_backend_blocks(int progress_fd, long long total_blocks, ino_t inode,
		char delete_which_one);

void busy_wait_all_specified_upload_threads(ino_t inode);

int revert_block_status_LDISK(ino_t this_inode, long long blockno,
		int e_index, long long page_filepos);
