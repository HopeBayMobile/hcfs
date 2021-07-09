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
#include "meta_mem_cache.h"
#include "dir_lookup_params.h"

META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	META_CACHE_ENTRY_STRUCT *ret_entry = 
		(META_CACHE_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
	ret_entry->inode_num = this_inode;
	return ret_entry;
}

int32_t meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	return 0;
}

int32_t meta_cache_seek_dir_entry(ino_t this_inode, DIR_ENTRY_PAGE *result_page,
		        int32_t *result_index, const char *childname,
			META_CACHE_ENTRY_STRUCT *body_ptr)
{
	switch (body_ptr->inode_num) {
	case 1:
		if(strcmp("file1", childname))
			break;
		result_page->dir_entries[0].d_ino = INO__FILE1_FOUND;
		*result_index = 0;
		return 0;
	case INO__FILE1_FOUND:
		if(strcmp("file2", childname))
			break;
		result_page->dir_entries[1].d_ino = INO__FILE2_FOUND;
		*result_index = 1;
		return 0;
	case INO__FILE2_FOUND:
		if(strcmp("file3", childname))
			break;
		result_page->dir_entries[2].d_ino = INO__FILE3_FOUND;
		*result_index = 2;
		return 0;
	case INO__FILE3_FOUND:
		if(strcmp("file4", childname))
			break;
		result_page->dir_entries[3].d_ino = INO__FILE4_FOUND;
		*result_index = 3;
		return 0;
	default:
		return -1;
	}
	return -1;
}

int32_t meta_cache_close_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	free(body_ptr);
	return 0;
}
