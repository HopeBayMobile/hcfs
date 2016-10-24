#include "block_iterator.h"

/**
	Usage:
	FILE_BLOCK_ITERATOR *now;
	FILE_BLOCK_ITERATOR *iter = init_block_iter(fptr);
	if (!iter)
		return -ENOMEM;
	for (now = iter->begin(iter); now; now = iter->next(iter)) {
		// int64_t now_block is now->now_block;
		// BLOCK_ENTRY_PAGE page is now->page;
	}
	if (iter->errcode < 0) {
		// Error occur
	}
	destroy_block_iter(iter);
 */
FILE_BLOCK_ITERATOR *init_block_iter(FILE *fptr)
{
	FILE_BLOCK_ITERATOR *iter;

	iter = (FILE_BLOCK_ITERATOR *) calloc(sizeof(FILE_BLOCK_ITERATOR), 1);
	if (!iter) {
		write_log(0, "Error: Fail to alloc mem in %s", __func__);
		return NULL;
	}

	FSEEK(fptr, 0, SEEK_SET);
	FREAD(&(iter->filestat), sizeof(HCFS_STAT), 1, fptr);
	FREAD(&(iter->filemeta), sizeof(FILE_META_TYPE), 1, fptr);

	iter->total_blocks = iter->filestat.size == 0 ? 0 :
			((iter->filestat.size - 1) / MAX_BLOCK_SIZE) + 1;
	iter->now_block = -1;
	iter->now_page = -1;
	/* Pointer */
	iter->fptr = fptr;
	iter->begin = begin_block;
	iter->next = next_block;
	iter->jump = goto_block;
	return iter;

errcode_handle:
	destroy_block_iter(iter);
	return NULL;
}

void destroy_block_iter(FILE_BLOCK_ITERATOR *iter)
{
	FREE(iter);
}

FILE_BLOCK_ITERATOR *next_block(FILE_BLOCK_ITERATOR *iter)
{
	int64_t count, which_page, ret_size;
	int32_t e_index, ret, errcode;

	for (count = iter->now_block + 1; count < iter->total_blocks; count++) {
		e_index = count % MAX_BLOCK_ENTRIES_PER_PAGE;
		which_page = count / MAX_BLOCK_ENTRIES_PER_PAGE;
		if (which_page == iter->now_page)
			break;
		page_pos = seek_page2(&(iter->filemeta), fptr, which_page, 0);
		if (page_pos <= 0) {
			count += (MAX_BLOCK_ENTRIES_PER_PAGE - 1);
		} else {
			FSEEK(fptr, page_pos, SEEK_SET);
			FREAD(&(iter->page), sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
			iter->page_pos = page_pos;
			iter->now_page = which_page;
			break;
		}
	}
	if (count >= total_blocks)
		return NULL;

	iter->e_index = e_index;
	iter->now_block = count;
	return iter;

errcode_handle:
	iter->errcode = errcode;
	return NULL;
}

FILE_BLOCK_ITERATOR *goto_block(FILE_BLOCK_ITERATOR *iter, int64_t block_no)
{
	int64_t which_page, ret_size;
	int32_t e_index, errcode, ret;

	if (block_no < 0 || block_no >= iter->total_blocks)
		return NULL;

	e_index = block_no % MAX_BLOCK_ENTRIES_PER_PAGE;
	which_page = block_no / MAX_BLOCK_ENTRIES_PER_PAGE;
	if (which_page != iter->now_page) {
		page_pos = seek_page2(&(iter->filemeta), fptr, which_page, 0);
		if (page_pos > 0) {
			FSEEK(fptr, page_pos, SEEK_SET);
			FREAD(&(iter->page), sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
			iter->page_pos = page_pos;
			iter->now_page = which_page;
		} else {
			return NULL;
		}
	}

	iter->e_index = e_index;
	iter->now_block = block_no;
	return iter;

errcode_handle:
	iter->errcode = errcode;
	return NULL;
}

FILE_BLOCK_ITERATOR *begin_block(FILE_BLOCK_ITERATOR *iter)
{
	iter->now_block = -1;
	iter->now_page = -1;
	iter->e_index = 0;
	iter->page_pos = 0;
	return next_block(iter);
}
