#include <sys/stat.h>
#include <stdio.h>

#include "mock_param.h"

#include "meta_mem_cache.h"
#include "params.h"

int meta_cache_lookup_dir_data(ino_t this_inode, struct stat *inode_stat,
	DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page,
	META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}


int meta_cache_update_dir_data(ino_t this_inode, const struct stat *inode_stat,
    const DIR_META_TYPE *dir_meta_ptr, const DIR_ENTRY_PAGE *dir_page,
    META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}


META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	return 0;
}

int meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	return 0;
}


int meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}


int meta_cache_close_file(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	return 0;
}


int meta_cache_lookup_file_data(ino_t this_inode, struct stat *inode_stat,
	FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
		long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int meta_cache_update_file_data(ino_t this_inode, const struct stat *inode_stat,
    const FILE_META_TYPE *file_meta_ptr, const BLOCK_ENTRY_PAGE *block_page,
    const long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int dir_add_entry(ino_t parent_inode, ino_t child_inode, char *childname,
	mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int dir_remove_entry(ino_t parent_inode, ino_t child_inode, char *childname, 
	mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	strcpy(pathname, META_PATH);
	return 0;
}

int init_dir_page(DIR_ENTRY_PAGE *tpage, ino_t self_inode, ino_t parent_inode, 
	long long this_page_pos)
{
	return 0;
}

int decrease_nlink_inode_file(ino_t this_inode)
{
	return 0;
}

int mark_inode_delete(ino_t this_inode)
{
	return 0;
}
