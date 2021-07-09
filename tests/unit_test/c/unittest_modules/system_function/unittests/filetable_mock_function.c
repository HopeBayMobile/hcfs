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
#include "mock_params.h"

META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	META_CACHE_ENTRY_STRUCT *ret;

	switch (this_inode) {
	case INO__META_CACHE_LOCK_ENTRY_FAIL:
		return NULL;
	case INO__META_CACHE_LOCK_ENTRY_SUCCESS:
		ret = (META_CACHE_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		ret->inode_num = INO__META_CACHE_LOCK_ENTRY_SUCCESS;
		return ret;
	default:
		ret = (META_CACHE_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		ret->inode_num = this_inode;
		return ret;
	}
}

int32_t meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int32_t meta_cache_close_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int32_t write_log(int32_t level, const char *format, ...)
{
	return 0;
}

