/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: xattr_ops.h
* Abstract: The header file for xattr structure and definition
*
* Revision History
* 2015/6/15 Kewei created the header file
* 2015/6/16 Kewei defined the xattr structure
*
**************************************************************************/


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
