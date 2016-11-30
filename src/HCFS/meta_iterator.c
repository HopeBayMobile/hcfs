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

