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

#define MAX_KEY_SIZE 256 /* Max key length */
#define MAX_VALUE_BLOCK_SIZE 8192 /* Max value size per block(8KB) */
#define MAX_KEY_ENTRY_PER_LIST 55 /* Max key entry of the sorted array (55) */
#define MAX_KEY_HASH_ENTRY 64 /* Max hash table entries (64) */

/* Define namespace of xattr */
#define USER 0
#define SYSTEM 1
#define SECURITY 2
#define TRUSTED 3

/* Struct of VALUE_BLOCK. Value of an extened attr is stored using linked
   VALUE_BLOCK, and it will be reclaimed if xattr is removed. */
typedef struct {
	char content[MAX_VALUE_BLOCK_SIZE]; /* Content is NOT null-terminated */
	long long next_block_pos;
} VALUE_BLOCK;

/* A key entry includes key size, value size, the key string, and a file
   offset pointing to first value block. */
typedef struct {
	unsigned key_size;
	unsigned value_size;
	char key[MAX_KEY_SIZE]; /* Key is null-terminated string  */
	long long first_value_block_pos;
} KEY_ENTRY;

/* KEY_LIST includes an array sorted by key, and number of xattr.
   If the KEY_LIST is the first one, prev_list_pos is set to 0. If it is the
   last one, then next_list_pos is set to 0. */
typedef struct {
	unsigned num_xattr;
	KEY_ENTRY key_list[MAX_KEY_ENTRY_PER_LIST];
	long long next_list_pos;
} KEY_LIST_PAGE;

/* NAMESPACE_PAGE includes a hash table which is used to hash the input key.
   Each hash entry points to a KEY_LIST. */
typedef struct {
	unsigned num_xattr;
	long long key_hash_table[MAX_KEY_HASH_ENTRY];
} NAMESPACE_PAGE;

/* XATTR_PAGE is pointed by next_xattr_page in meta file. Namespace is one of
   user, system, security, and trusted. */
typedef struct {
	long long reclaimed_key_list_page;
	long long reclaimed_value_block;
	NAMESPACE_PAGE namespace_page[4];
} XATTR_PAGE;

int parse_xattr_namespace(const char *name, char *name_space, char *key);

int insert_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, const long long xattr_filepos,
	const char name_space, const char *key,
	const char *value, const size_t size, const int flag);

int get_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page,
	const char name_space, const char *key, char *value, const size_t size,
	size_t *actual_size);

int list_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, char *key_buf, const size_t size,
	size_t *actual_size);

int remove_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, const long long xattr_filepos,
	const char name_space, const char *key);

int find_key_entry(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	long long first_key_list_pos, KEY_LIST_PAGE *target_key_list_page,
	int *key_index, long long *target_key_list_pos, const char *key,
	KEY_LIST_PAGE *prev_page, long long *prev_pos);

#endif
