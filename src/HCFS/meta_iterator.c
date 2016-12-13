/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: meta_iterator.c
* Abstract: The c source file for iterator operations
*
* Revision History
* 2016/10/20 Kewei created this file.
*
**************************************************************************/

#include "meta_iterator.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "metaops.h"

/*********************************
 *
 *
 * Method of file block iterator
 *
 *
 *********************************/

/**
 * Initilize block iterator using parameter "fptr", which is a file pointer of
 * the meta file. This function will NOT lock the meta file so it should be
 * locked by caller if race condition may occur when using this block iterator.
 *
 * @param fptr File pointer of the meta file to be iterated.
 *
 * @return pointer of the iterator. return NULL in case that error happened.
 */
FILE_BLOCK_ITERATOR *init_block_iter(FILE *fptr)
{
	FILE_BLOCK_ITERATOR *iter;
	int64_t ret_size;
	int32_t ret, errcode;

	iter = (FILE_BLOCK_ITERATOR *) calloc(sizeof(FILE_BLOCK_ITERATOR), 1);
	if (!iter) {
		write_log(0, "Error: Fail to alloc mem in %s. Code %d",
				__func__, errno);
		return NULL;
	}

	FSEEK(fptr, 0, SEEK_SET);
	FREAD(&(iter->filestat), sizeof(HCFS_STAT), 1, fptr);
	FREAD(&(iter->filemeta), sizeof(FILE_META_TYPE), 1, fptr);

	iter->total_blocks = iter->filestat.size == 0 ? 0 :
			((iter->filestat.size - 1) / MAX_BLOCK_SIZE) + 1;
	iter->now_block_no = -1;
	iter->now_page_no = -1;
	iter->now_bentry = NULL;
	/* Pointer */
	iter->fptr = fptr;
	iter->base.begin = (void *)&begin_block;
	iter->base.next = (void *)&next_block;
	iter->base.jump = (void *)&goto_block;
	return iter;

errcode_handle:
	errno = -errcode;
	destroy_block_iter(iter);
	return NULL;
}

/**
 * Go to next block entry.
 *
 * @param iter Iterator structure of the file meta.
 *
 * @return this iterator itself. Otherwise return NULL and error code
 *         is recorded in errno.
 */
FILE_BLOCK_ITERATOR *next_block(FILE_BLOCK_ITERATOR *iter)
{
	int64_t count, which_page, ret_size, page_pos;
	int32_t e_index, ret, errcode;

	for (count = iter->now_block_no + 1; count < iter->total_blocks; count++) {
		e_index = count % MAX_BLOCK_ENTRIES_PER_PAGE;
		which_page = count / MAX_BLOCK_ENTRIES_PER_PAGE;
		if (which_page == iter->now_page_no)
			break;
		page_pos = seek_page2(&(iter->filemeta), iter->fptr,
				which_page, 0);
		if (page_pos <= 0) {
			count += (MAX_BLOCK_ENTRIES_PER_PAGE - 1);
		} else {
			FSEEK(iter->fptr, page_pos, SEEK_SET);
			FREAD(&(iter->page), sizeof(BLOCK_ENTRY_PAGE), 1,
					iter->fptr);
			iter->page_pos = page_pos;
			iter->now_page_no = which_page;
			break;
		}
	}
	if (count >= iter->total_blocks) {
		errno = ENOENT;
		return NULL;
	}

	iter->e_index = e_index;
	iter->now_block_no = count;
	iter->now_bentry = iter->page.block_entries + e_index;
	return iter;

errcode_handle:
	errno = -errcode;
	return NULL;
}

/**
 * Jump to some specified block.
 *
 * @param iter Iterator structure of the file meta.
 * @param block_no Index of the target block.
 *
 * @return this iterator itself. Otherwise return NULL and error code
 *         is recorded in errno.
 */
FILE_BLOCK_ITERATOR *goto_block(FILE_BLOCK_ITERATOR *iter, int64_t block_no)
{
	int64_t which_page, ret_size, page_pos;
	int32_t e_index, errcode, ret;

	if (block_no < 0 || block_no >= iter->total_blocks) {
		errno = ENOENT;
		return NULL;
	}

	e_index = block_no % MAX_BLOCK_ENTRIES_PER_PAGE;
	which_page = block_no / MAX_BLOCK_ENTRIES_PER_PAGE;
	if (which_page != iter->now_page_no) {
		page_pos = seek_page2(&(iter->filemeta), iter->fptr,
				which_page, 0);
		if (page_pos > 0) {
			FSEEK(iter->fptr, page_pos, SEEK_SET);
			FREAD(&(iter->page), sizeof(BLOCK_ENTRY_PAGE), 1,
					iter->fptr);
			iter->page_pos = page_pos;
			iter->now_page_no = which_page;
		} else {
			errno = ENOENT;
			return NULL;
		}
	}

	iter->e_index = e_index;
	iter->now_block_no = block_no;
	iter->now_bentry = iter->page.block_entries + e_index;
	return iter;

errcode_handle:
	errno = -errcode;
	return NULL;
}

/**
 * Jump to first block. It is the first entry of existed page.
 *
 * @param iter Iterator structure of the file meta.
 *
 * @return this iterator itself. Otherwise return NULL and error code
 *         is recorded in errno.
 */
FILE_BLOCK_ITERATOR *begin_block(FILE_BLOCK_ITERATOR *iter)
{
	iter->now_block_no = -1;
	iter->now_page_no = -1;
	iter->e_index = 0;
	iter->page_pos = 0;
	iter->now_bentry = NULL;
	return next_block(iter);
}

/**
 * Free resource about block iterator.
 *
 * @param iter Iterator structure of the file meta.
 *
 * @return none.
 */
void destroy_block_iter(FILE_BLOCK_ITERATOR *iter)
{
	free(iter);
}

/*********************************
 *
 *
 * Method of hash list iterator
 *
 *
 *********************************/

/**
 * Initialize iterator of hash list structure.
 *
 * @param hash_list Pointer of the hash list structure.
 *
 * @return iterator of the hash list. Return null on error, and error code
 *         is recorded in errno.
 */
HASH_LIST_ITERATOR *init_hashlist_iter(HASH_LIST *hash_list)
{
	HASH_LIST_ITERATOR *iter;

	iter = (HASH_LIST_ITERATOR *) calloc(sizeof(HASH_LIST_ITERATOR), 1);
	if (!iter) {
		write_log(0, "Error: Fail to alloc mem in %s. Code %d",
				__func__, errno);
		return NULL;
	}

	iter->hash_list = hash_list;
	iter->base.next = (void *)&next_entry;
	iter->base.begin = (void *)&begin_entry;
	iter->now_bucket_idx = -1;
	iter->now_node = NULL;
	iter->now_key = NULL;
	iter->now_data = NULL;
	return iter;
}

/**
 * Go to next entry. It traverse hash table from first element with index 0
 * to last one.
 *
 * @param iter Iterator of hash list.
 *
 * @return iterator of the hash list. Return null on no entry or error,
 *         and error code is recorded in errno.
 */
HASH_LIST_ITERATOR *next_entry(HASH_LIST_ITERATOR *iter)
{
	uint32_t idx;
	LIST_NODE *next_node = NULL;

	/* Try next node in this bucket */
	if (iter->now_node) {
		next_node = iter->now_node->next;
		if (next_node) {
			iter->now_node = next_node;
			iter->now_key = next_node->key;
			iter->now_data = next_node->data;
			return iter;
		}
	}

	/* Try next bucket */
	for (idx = iter->now_bucket_idx + 1;
				idx < iter->hash_list->table_size; idx++) {
		next_node = iter->hash_list->hash_table[idx].first_entry;
		if (next_node)
			break;
		else
			continue;
	}
	if (next_node) {
		iter->now_bucket_idx = idx;
		iter->now_node = next_node;
		iter->now_key = next_node->key;
		iter->now_data = next_node->data;
		return iter;
	}

	errno = ENOENT;
	return NULL;
}

/**
 * Jump to first element of the hash list structure.
 *
 * @param iter Iterator of hash list.
 *
 * @return iterator of the hash list. Return null on no entry or error,
 *         and error code is recorded in errno.
 */
HASH_LIST_ITERATOR *begin_entry(HASH_LIST_ITERATOR *iter)
{
	iter->now_bucket_idx = -1;
	iter->now_node = NULL;
	iter->now_key = NULL;
	iter->now_data = NULL;

	return next_entry(iter);
}

/**
 * Free resource of the hash list iterator.
 *
 * @param iter Iterator of hash list.
 *
 * @return none.
 */
void destroy_hashlist_iter(HASH_LIST_ITERATOR *iter)
{
	free(iter);
}

/*********************************
 *
 *
 * Method of dir entry iterator
 *
 *
 *********************************/

/**
 * Initilize dir iterator using parameter "fptr", which is a file pointer of
 * the meta file. This function will NOT lock the meta file so it should be
 * locked by caller if race condition may occur when using this block iterator.
 *
 * @param fptr File pointer of the meta file to be iterated.
 *
 * @return pointer of the iterator. return NULL in case that error happened.
 */
DIR_ENTRY_ITERATOR *init_dir_iter(FILE *fptr)
{
	DIR_ENTRY_ITERATOR *iter;
	int64_t ret_size;
	int32_t ret, errcode;

	iter = (DIR_ENTRY_ITERATOR *) calloc(sizeof(DIR_ENTRY_ITERATOR), 1);
	if (!iter) {
		write_log(0, "Error: Fail to alloc mem in %s. Code %d",
				__func__, errno);
		return NULL;
	}

	FSEEK(fptr, sizeof(HCFS_STAT), SEEK_SET);
	FREAD(&(iter->dir_meta), sizeof(DIR_META_TYPE), 1, fptr);

	iter->fptr = fptr;
	iter->base.begin = (void *)&next_dir_entry;
	iter->base.next = (void *)&begin_dir_entry;
	iter->now_dirpage_pos = -1;
	iter->now_entry_idx = -1;
	iter->now_entry = NULL;
	return iter;

errcode_handle:
	errno = -errcode;
	destroy_dir_iter(iter);
	return NULL;
}

/**
 * Go to next dir entry. The iterator traverse dir structure using the
 * pointer "tree_walk_next" recorded in dir meta file. It go to next entry
 * instead of going ahead to next page if now page is not in the end.
 *
 * @param iter Pointer of the dir entry iterator.
 *
 * @return the iterator itself on success, otherwise NULL. Error code is
 *         recorded in errno.
 */
DIR_ENTRY_ITERATOR *next_dir_entry(DIR_ENTRY_ITERATOR *iter)
{
	int64_t now_page_pos;
	int32_t ret, errcode;
	size_t ret_size;
	FILE *fptr = iter->fptr;

	/* Check now page pos */
	if (iter->now_dirpage_pos == 0) {
		errno = ENOENT;
		return NULL;

	} else if (iter->now_dirpage_pos == -1) {
		/* Begin from first page */
		now_page_pos = iter->dir_meta.tree_walk_list_head;
		if (now_page_pos == 0) {
			errno = ENOENT;
			return NULL;
		}
		FSEEK(fptr, now_page_pos, SEEK_SET);
		FREAD(&(iter->now_page), sizeof(DIR_ENTRY_PAGE), 1, fptr);
		iter->now_dirpage_pos = now_page_pos;
		iter->now_entry_idx = -1;
	}

	/* Try next entry */
	if (iter->now_entry_idx + 1 < iter->now_page.num_entries) {
		iter->now_entry_idx += 1;
		iter->now_entry =
			&(iter->now_page.dir_entries[iter->now_entry_idx]);
		return iter;
	} else {
		iter->now_entry_idx = -1;
		iter->now_entry = NULL;
	}

	/* Go to next page */
	iter->now_dirpage_pos = iter->now_page.tree_walk_next;
	while (iter->now_dirpage_pos) {
		FSEEK(fptr, iter->now_dirpage_pos, SEEK_SET);
		FREAD(&(iter->now_page), sizeof(DIR_ENTRY_PAGE), 1, fptr);
		if (iter->now_page.num_entries > 0) {
			iter->now_entry_idx = 0;
			iter->now_entry =
			    &(iter->now_page.dir_entries[0]);
			    break;
		}

		iter->now_dirpage_pos = iter->now_page.tree_walk_next;
	}

	if (iter->now_dirpage_pos == 0) {
		errno = ENOENT;
		return NULL;
	}

	return iter;

errcode_handle:
	errno = -errcode;
	return NULL;
}

/**
 * Jump to first entry of "tree_walk_list_head".
 *
 * @param iter Pointer of the dir entry iterator.
 *
 * @return the iterator itself on success, otherwise NULL. Error code is
 *         recorded in errno.
 */
DIR_ENTRY_ITERATOR *begin_dir_entry(DIR_ENTRY_ITERATOR *iter)
{
	iter->now_dirpage_pos = -1;
	iter->now_entry_idx = -1;
	iter->now_entry = NULL;

	return next_dir_entry(iter);
}

/**
 * Free resource of iterator.
 *
 * @param iter Pointer of the dir entry iterator.
 *
 * @return none.
 */
void destroy_dir_iter(DIR_ENTRY_ITERATOR *iter) { free(iter); }
