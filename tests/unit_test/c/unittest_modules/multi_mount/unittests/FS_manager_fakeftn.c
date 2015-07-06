#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "fuseop.h"

int write_log(int level, char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

int search_dir_entry_btree(const char *target_name, DIR_ENTRY_PAGE *tnode,
		int fh, int *result_index, DIR_ENTRY_PAGE *result_node)
{
	return -ENOENT;
}

/* if returns 1, then there is an entry to be added to the parent */
int insert_dir_entry_btree(DIR_ENTRY *new_entry, DIR_ENTRY_PAGE *tnode,
	int fh, DIR_ENTRY *overflow_median, long long *overflow_new_page,
	DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
	long long *temp_child_page_pos)
{

	return 0;
}

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	return 0;
}

ino_t super_block_new_inode(struct stat *in_stat,
				unsigned long *ret_generation)
{
	return 2;
}

int super_block_mark_dirty(ino_t this_inode)
{
	return 0;
}
void set_timestamp_now(struct stat *thisstat, char mode)
{
	return 0;
}
int init_dir_page(DIR_ENTRY_PAGE *tpage, ino_t self_inode, ino_t parent_inode,
						long long this_page_pos)
{
	return 0;
}

