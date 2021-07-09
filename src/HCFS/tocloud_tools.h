/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
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
		int64_t page_pos, int64_t toupload_seq, char *blockid);

int delete_backend_blocks(int progress_fd, long long total_blocks, ino_t inode,
		char delete_which_one);

void busy_wait_all_specified_upload_threads(ino_t inode);

ino_t pull_retry_inode(IMMEDIATELY_RETRY_LIST *list);
void push_retry_inode(IMMEDIATELY_RETRY_LIST *list, ino_t inode);

#endif
