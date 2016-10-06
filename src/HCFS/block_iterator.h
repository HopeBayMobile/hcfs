#ifndef GW20_HCFS_BLOCK_ITERATOR_H_
#define GW20_HCFS_BLOCK_ITERATOR_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "meta.h"

typedef struct FILE_BLOCK_ITERATOR {
	int64_t page_pos;
	int64_t now_page;
	int32_t e_index;
	int64_t now_block;
	int64_t total_blocks;
	HCFS_STAT filestat;
	FILE_META_TYPE filemeta;
	BLOCK_ENTRY_PAGE page;
	FILE *fptr;
	FILE_BLOCK_ITERATOR *(*next)(FILE_BLOCK_ITERATOR *iter);
	FILE_BLOCK_ITERATOR *(*jump)(FILE_BLOCK_ITERATOR *iter, int64_t block_no);
} FILE_BLOCK_ITERATOR;

FILE_BLOCK_ITERATOR *init_block_iter(FILE *fptr);
void destroy_block_iter(FILE_BLOCK_ITERATOR *iter);
FILE_BLOCK_ITERATOR *next_block(FILE_BLOCK_ITERATOR *iter);
FILE_BLOCK_ITERATOR *goto_block(FILE_BLOCK_ITERATOR *iter, int64_t block_no);

#endif
