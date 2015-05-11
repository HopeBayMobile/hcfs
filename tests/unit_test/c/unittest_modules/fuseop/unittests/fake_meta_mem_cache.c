#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "meta_mem_cache.h"
#include "fake_misc.h"
#include "global.h"

int meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}
int meta_cache_close_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}
int meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	return 0;
}

int meta_cache_update_file_data(ino_t this_inode, const struct stat *inode_stat,
	const FILE_META_TYPE *file_meta_ptr, const BLOCK_ENTRY_PAGE *block_page,
	const long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	before_update_file_data = FALSE;
	if (inode_stat != NULL)
		memcpy(&updated_stat, inode_stat, sizeof(struct stat));

	return 0;
}

int meta_cache_lookup_file_data(ino_t this_inode, struct stat *inode_stat,
	FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
	long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (inode_stat != NULL) {
		switch (this_inode) {
		case 1:
			inode_stat->st_ino = 1;
			inode_stat->st_mode = S_IFDIR | 0700;
			inode_stat->st_atime = 100000;
			break;
		case 2:
			inode_stat->st_ino = 2;
			inode_stat->st_mode = S_IFREG | 0700;
			inode_stat->st_atime = 100000;
			break;
		case 4:
			inode_stat->st_ino = 4;
			inode_stat->st_mode = S_IFREG | 0700;
			inode_stat->st_atime = 100000;
			break;	
		case 6:
			inode_stat->st_ino = 6;
			inode_stat->st_mode = S_IFDIR | 0700;
			inode_stat->st_atime = 100000;
			break;	
		case 10:
			inode_stat->st_ino = 10;
			inode_stat->st_mode = S_IFREG | 0700;
			inode_stat->st_atime = 100000;
			break;
		case 11:
			inode_stat->st_ino = 11;
			inode_stat->st_mode = S_IFREG | 0700;
			inode_stat->st_atime = 100000;
			inode_stat->st_size = 1024;
			break;
		case 12:
			inode_stat->st_ino = 12;
			inode_stat->st_mode = S_IFDIR | 0700;
			inode_stat->st_atime = 100000;
			break;
		case 13:
			inode_stat->st_ino = 13;
			inode_stat->st_mode = S_IFDIR | 0700;
			inode_stat->st_atime = 100000;
			break;
		case 14:
			inode_stat->st_ino = 14;
			inode_stat->st_mode = S_IFREG | 0700;
			inode_stat->st_atime = 100000;
			inode_stat->st_size = 102400;
			break;
		case 15:
			inode_stat->st_ino = 15;
			inode_stat->st_mode = S_IFREG | 0700;
			inode_stat->st_atime = 100000;
			inode_stat->st_size = 204800;
			break;
		default:
			break;
		}

		if (before_update_file_data == FALSE)
			memcpy(inode_stat, &updated_stat, sizeof(struct stat));
	}


	if (file_meta_ptr != NULL) {
		file_meta_ptr->next_block_page = 0;
		switch (this_inode) {
		case 14:
		case 15:
			file_meta_ptr->next_block_page = sizeof(struct stat);
			break;
		default:
			break;
		}
	}

	if (block_page != NULL) {
		memset(block_page, 0, sizeof(BLOCK_ENTRY_PAGE));
		switch (this_inode) {
		case 14:
		case 15:
			if (page_pos == sizeof(struct stat)) {
				block_page->num_entries = 1;
				block_page->block_entries[0].status
					= fake_block_status;
				block_page->next_page = 0;
			}
			break;
		default:
			break;
		}
	}
	return 0;
}

int meta_cache_update_dir_data(ino_t this_inode, const struct stat *inode_stat,
	const DIR_META_TYPE *dir_meta_ptr, const DIR_ENTRY_PAGE *dir_page,
	META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int meta_cache_lookup_dir_data(ino_t this_inode, struct stat *inode_stat,
	DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page,
	META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (dir_meta_ptr != NULL) {
		if (this_inode == 13)
			dir_meta_ptr->total_children = 2;
		else
			dir_meta_ptr->total_children = 0;
	}
	return 0;
}

META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	return NULL;
}

int meta_cache_drop_pages(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int init_meta_cache_headers(void)
{
	return 0;
}

int release_meta_cache_headers(void)
{
	return 0;
}

int meta_cache_remove(ino_t this_inode)
{
	return 0;
}

int meta_cache_seek_dir_entry(ino_t this_inode, DIR_ENTRY_PAGE *result_page,
	int *result_index, char *childname, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	switch (this_inode) {
	case 1:
		if (strcmp(childname,"testfile") == 0) {
			result_page->dir_entries[0].d_ino = 2;
			*result_index = 0;
			break;
		}
		if (strcmp(childname,"testsamefile") == 0) {
			result_page->dir_entries[0].d_ino = 2;
			*result_index = 0;
			break;
		}
		if (strcmp(childname,"testfile1") == 0) {
			result_page->dir_entries[0].d_ino = 10;
			*result_index = 0;
			break;
		}
		if (strcmp(childname,"testfile2") == 0) {
			result_page->dir_entries[0].d_ino = 11;
			*result_index = 0;
			break;
		}
		if (strcmp(childname,"testdir1") == 0) {
			result_page->dir_entries[0].d_ino = 12;
			*result_index = 0;
			break;
		}
		if (strcmp(childname,"testdir2") == 0) {
			result_page->dir_entries[0].d_ino = 13;
			*result_index = 0;
			break;
		}
		break;
	default:
		break;
	}
	return 0;
}
