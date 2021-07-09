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


#ifndef GW20_XATTR_OPS_H_
#define GW20_XATTR_OPS_H_

#include "meta_mem_cache.h"
#include "meta.h"

/* Define namespace of xattr */
#define USER 0
#define SYSTEM 1
#define SECURITY 2
#define TRUSTED 3

enum {
	READ_XATTR, WRITE_XATTR
};


int32_t parse_xattr_namespace(const char *name, char *name_space, char *key);

int32_t insert_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, const int64_t xattr_filepos,
	const char name_space, const char *key,
	const char *value, const size_t size, const int32_t flag);

int32_t get_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page,
	const char name_space, const char *key, char *value, const size_t size,
	size_t *actual_size);

int32_t list_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, char *key_buf, const size_t size,
	size_t *actual_size);

int32_t remove_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, const int64_t xattr_filepos,
	const char name_space, const char *key);

int32_t find_key_entry(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	int64_t first_key_list_pos, KEY_LIST_PAGE *target_key_list_page,
	int32_t *key_index, int64_t *target_key_list_pos, const char *key,
	KEY_LIST_PAGE *prev_page, int64_t *prev_pos);

#endif
