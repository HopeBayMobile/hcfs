#include "block_iterator.h"

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
	iter->fptr = fptr;
	iter->begin = begin_block;
	iter->next = next_block;
	iter->jump = goto_block;

	return iter;
}

void destroy_block_iter(FILE_BLOCK_ITERATOR *iter)
{
	free(iter);
}

FILE_BLOCK_ITERATOR *next_block(FILE_BLOCK_ITERATOR *iter)
{
	int64_t count, which_page;
	int32_t e_index;

	for (count = iter->now_block + 1; count < total_blocks; count++) {
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
			break;
		}
	}
	if (count >= total_blocks)
		return NULL;

	iter->page_pos = page_pos;
	iter->now_page = which_page;
	iter->e_index = e_index;
	iter->now_block = count;
	return iter;
}

FILE_BLOCK_ITERATOR *goto_block(FILE_BLOCK_ITERATOR *iter, int64_t block_no)
{

}
