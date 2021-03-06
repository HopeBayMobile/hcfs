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
#include "fuseop.h"
#include "hcfs_cachebuild.h"
#include "mock_params.h"
#include "super_block.h"
#include <inttypes.h>
#include <stdarg.h>
#include <string.h>

void init_mock_system_config()
{
	system_config->blockpath = malloc(sizeof(char) * 100);
	strcpy(system_config->blockpath, "/tmp/testHCFS/blockpath");
}

int32_t fetch_block_path(char *pathname, ino_t this_inode, int64_t block_num)
{
	char block_name[200];

	sprintf(block_name,
		"/tmp/testHCFS/run_cache_loop_block%" PRIu64 "_%" PRId64,
		(uint64_t)this_inode, block_num);
	strcpy(pathname, block_name);
	
	return 0;
}

int32_t fetch_meta_path(char *pathname, ino_t this_inode)
{
	char meta_name[200];

	sprintf(meta_name, "/tmp/testHCFS/run_cache_loop_filemeta%" PRIu64 "",
			(uint64_t)this_inode);
	strcpy(pathname, meta_name);

	return 0;
}

int32_t build_cache_usage(void)
{
	return 0;
}

int32_t super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	if (inode_ptr == NULL)
		inode_ptr = (SUPER_BLOCK_ENTRY *) malloc(sizeof(SUPER_BLOCK_ENTRY));
	
	inode_ptr->inode_stat.ino = 1;
	inode_ptr->inode_stat.mode = S_IFREG;

	return 0;
}

int32_t sync_hcfs_system_data(char need_lock)
{
	return 0;
}

int32_t super_block_mark_dirty(ino_t this_inode)
{
	return 0;	
}

CACHE_USAGE_NODE *return_cache_usage_node(ino_t this_inode)
{
	if (this_inode > 0)
		nonempty_cache_hash_entries--;
	
	return NULL;
}

int64_t seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr, 
	int64_t target_page, int64_t hint_page)
{	
	int64_t ret = sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) + 
		target_page * sizeof(BLOCK_ENTRY_PAGE);
	
	return ret;
}

int32_t write_log(int32_t level, const char *format, ...)
{
	va_list ap;

	/*va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);*/

	return 0;
}
int32_t update_file_stats(FILE *metafptr, int64_t num_blocks_delta,
			int64_t num_cached_blocks_delta,
			int64_t cached_size_delta,
			int64_t dirty_data_size_delta,
			ino_t thisinode)
{
	return 0;
}
int32_t restore_meta_super_block_entry(ino_t this_inode,
                struct stat *ret_stat)
{
	return 0;
}

int32_t change_system_meta(int64_t system_data_size_delta,
		int64_t meta_size_delta, int64_t cache_data_size_delta,
		int64_t cache_blocks_delta, int64_t dirty_cache_delta,
		int64_t unpin_dirty_delta, BOOL need_sync)
{
	hcfs_system->systemdata.cache_size +=
				cache_data_size_delta;
	hcfs_system->systemdata.cache_blocks += cache_blocks_delta;
	return 0;
}
