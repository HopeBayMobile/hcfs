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

#ifndef SRC_HCFS_META_ITERATOR_H_
#define SRC_HCFS_META_ITERATOR_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "meta.h"
#include "hash_list_struct.h"

/**
 * file block iterator, directory entry iterator, and hash list entry iterator
 * is implemented.
 */

typedef struct ITERATOR_BASE {
	void *(*begin)(void *iter);
	void *(*next)(void *iter);
	void *(*jump)(void *iter, int64_t target_idx);
} ITERATOR_BASE;

#define iter_begin(iter)                                                       \
	((iter && iter->base.begin) ? iter->base.begin(iter) : ({              \
		errno = EINVAL;                                                \
		(void *)NULL;                                                  \
	}))
#define iter_next(iter)                                                        \
	((iter && iter->base.next) ? iter->base.next(iter) : ({                \
		errno = EINVAL;                                                \
		(void *)NULL;                                                  \
	}))
#define iter_jump(iter, elem_idx)                                              \
	((iter && iter->base.jump) ? iter->base.jump(iter, elem_idx) : ({      \
		errno = EINVAL;                                                \
		(void *)NULL;                                                  \
	}))

/**
 *	Usage:
 *	FILE_BLOCK_ITERATOR *iter = init_block_iter(fptr);
 *	if (!iter)
 *		return -errno;
 *	while (iter_next(iter) != NULL) {
 *		// Now block index is iter->now_block_no;
 *		// Now page is iter->page;
 *		// Now entry is iter->now_bentry;
 *	}
 *	if (errno != ENOENT) {
 *		// Error occur
 *	}
 *	destroy_block_iter(iter);
 */
typedef struct FILE_BLOCK_ITERATOR {
	ITERATOR_BASE base;
	int64_t total_blocks;
	int64_t now_page_no; /* page number */
	int32_t e_index; /* entry index */
	int64_t now_block_no; /* block number */
	int64_t page_pos; /* block page position in file */
	HCFS_STAT filestat;
	FILE_META_TYPE filemeta;
	BLOCK_ENTRY_PAGE page;
	BLOCK_ENTRY *now_bentry;
	FILE *fptr;
} FILE_BLOCK_ITERATOR;

FILE_BLOCK_ITERATOR *init_block_iter(FILE *fptr);
void destroy_block_iter(FILE_BLOCK_ITERATOR *iter);
FILE_BLOCK_ITERATOR *next_block(FILE_BLOCK_ITERATOR *iter);
FILE_BLOCK_ITERATOR *goto_block(FILE_BLOCK_ITERATOR *iter, int64_t block_no);
FILE_BLOCK_ITERATOR *begin_block(FILE_BLOCK_ITERATOR *iter);

/**
 *	Usage:
 *	HASH_LIST_ITERATOR *iter = init_hashlist_iter(fptr);
 *	if (!iter)
 *		return -errno;
 *	while (iter_next(iter) != NULL) {
 *		// iter->now_data
 *	}
 *	if (errno != ENOENT) {
 *		// Error occur
 *	}
 *	destroy_hashlist_iter(iter);
 */
typedef struct HASH_LIST_ITERATOR {
	ITERATOR_BASE base;
	HASH_LIST *hash_list;
	uint32_t now_bucket_idx;
	LIST_NODE *now_node;
	void *now_key;
	void *now_data;
} HASH_LIST_ITERATOR;

HASH_LIST_ITERATOR *init_hashlist_iter(HASH_LIST *hash_list);
HASH_LIST_ITERATOR *next_entry(HASH_LIST_ITERATOR *iter);
HASH_LIST_ITERATOR *begin_entry(HASH_LIST_ITERATOR *iter);
void destroy_hashlist_iter(HASH_LIST_ITERATOR *iter);

/**
 *	Usage:
 *	DIR_ENTRY_ITERATOR *iter = init_dir_iter(fptr);
 *	if (!iter)
 *		return -errno;
 *	while (iter_next(iter) != NULL) {
 *		// iter->now_data
 *	}
 *	if (errno != ENOENT) {
 *		// Error occur
 *	}
 *	destroy_dir_iter(iter);
 */
typedef struct DIR_ENTRY_ITERATOR {
	ITERATOR_BASE base;
	int64_t now_dirpage_pos;
	int32_t now_entry_idx;
	DIR_META_TYPE dir_meta;
	DIR_ENTRY_PAGE now_page;
	DIR_ENTRY *now_entry;
	FILE *fptr;
} DIR_ENTRY_ITERATOR;

DIR_ENTRY_ITERATOR *init_dir_iter(FILE *fptr);
DIR_ENTRY_ITERATOR *next_dir_entry(DIR_ENTRY_ITERATOR *iter);
DIR_ENTRY_ITERATOR *begin_dir_entry(DIR_ENTRY_ITERATOR *iter);
void destroy_dir_iter(DIR_ENTRY_ITERATOR *iter);

#endif  // SRC_HCFS_META_ITERATOR_H_
