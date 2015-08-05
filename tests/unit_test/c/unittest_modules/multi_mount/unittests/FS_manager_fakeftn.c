#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "fuseop.h"
#include "global.h"
#include "hcfscurl.h"
#include "FS_manager_unittest.h"

extern SYSTEM_CONF_STRUCT system_config;

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
	if (entry_in_database == FALSE)
		return -ENOENT;
	result_node->num_entries = 1;
	result_node->dir_entries[0].d_ino = fakeino;
	strcpy((result_node->dir_entries[0]).d_name, target_name);
	*result_index = 0;
	return 0;
}

/* if returns 1, then there is an entry to be added to the parent */
int insert_dir_entry_btree(DIR_ENTRY *new_entry, DIR_ENTRY_PAGE *tnode,
	int fh, DIR_ENTRY *overflow_median, long long *overflow_new_page,
	DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
	long long *temp_child_page_pos)
{
	off_t tmp_pos;
	DIR_ENTRY_PAGE tmppage;

	if (treesplit == FALSE) {
		memcpy(&(tnode->dir_entries[tnode->num_entries]), new_entry,
			sizeof(DIR_ENTRY));
		tnode->num_entries++;
		pwrite(fh, tnode, sizeof(DIR_ENTRY_PAGE), tnode->this_page_pos);
		return 0;
	}

	tmp_pos = lseek(fh, 0, SEEK_END);
	memset(&tmppage, 0, sizeof(DIR_ENTRY_PAGE));
	tmppage.this_page_pos = tmp_pos;
	memcpy(&(tmppage.dir_entries[tmppage.num_entries]), new_entry,
			sizeof(DIR_ENTRY));
	tmppage.num_entries++;
	tmppage.tree_walk_next = this_meta->tree_walk_list_head;
	this_meta->tree_walk_list_head = tmp_pos;
	pwrite(fh, &tmppage, sizeof(DIR_ENTRY_PAGE), tmp_pos);

	tnode->num_entries--;
	memcpy(overflow_median, &(tnode->dir_entries[tnode->num_entries]),
		sizeof(DIR_ENTRY));
	tnode->tree_walk_prev = tmp_pos;
	pwrite(fh, tnode, sizeof(DIR_ENTRY_PAGE), tnode->this_page_pos);

	*overflow_new_page = tmp_pos;
	return 1;
}

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	snprintf(pathname, 100, "%s/meta%ld", METAPATH, this_inode);
	return 0;
}

ino_t super_block_new_inode(struct stat *in_stat,
				unsigned long *ret_generation)
{
	return fakeino;
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
int hcfs_init_backend(CURL_HANDLE *curl_handle)
{
	if (failedcurlinit == TRUE)
		return 404;
	return 200;
}
void hcfs_destroy_backend(CURL *curl)
{
	return 200;
}
int hcfs_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
{
	if (failedput == TRUE)
		return 404;
	fseek(fptr, 0, SEEK_SET);
	fread(&headbuf, sizeof(DIR_META_TYPE), 1, fptr);
	return 200;
}
int hcfs_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
{
	if (failedget == TRUE)
		return 404;
	return 200;
}
int FS_is_mounted(char *fsname)
{

	return -ENOENT;
}

int delete_inode_meta(ino_t this_inode)
{
	char tmppath[100];

	snprintf(tmppath, 100, "%s/meta%ld", METAPATH, this_inode);
	unlink(tmppath);

	return 0;
}
int delete_dir_entry_btree(DIR_ENTRY *to_delete_entry, DIR_ENTRY_PAGE *tnode,
	int fh, DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
	long long *temp_child_page_pos)
{
	return 0;
}

int update_FS_statistics(char *pathname, long long system_size,
		long long num_inodes)
{
	return 0;
}


