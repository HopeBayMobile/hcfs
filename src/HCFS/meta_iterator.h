#ifndef GW20_HCFS_HCFS_ITERATOR_H_
#define GW20_HCFS_HCFS_ITERATOR_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "meta.h"

typedef struct ITERATOR_BASE {
	void *(*begin)(void *iter);
	void *(*next)(void *iter);
	void *(*jump)(void *iter, int64_t target_idx);
} ITERATOR_BASE;

/**
 *	Usage:
 *	FILE_BLOCK_ITERATOR *iter = init_block_iter(fptr);
 *	if (!iter)
 *		return -errno;
 *	while (iter_next(iter) != NULL) {
 *		// Now block index is iter->now_block_no;
 *		// Now page is iter->page;
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

#define iter_begin(iter) iter->base.begin(iter)
#define iter_next(iter) iter->base.next(iter)

FILE_BLOCK_ITERATOR *init_block_iter(FILE *fptr);
void destroy_block_iter(FILE_BLOCK_ITERATOR *iter);
FILE_BLOCK_ITERATOR *next_block(FILE_BLOCK_ITERATOR *iter);
FILE_BLOCK_ITERATOR *goto_block(FILE_BLOCK_ITERATOR *iter, int64_t block_no);
FILE_BLOCK_ITERATOR *begin_block(FILE_BLOCK_ITERATOR *iter);

#endif
