#include <sys/stat.h>
#include <stdio.h>

#include "mock_param.h"

#include "xattr_ops.h"
#include "meta_mem_cache.h"
#include "params.h"

int meta_cache_lookup_dir_data(ino_t this_inode, struct stat *inode_stat,
	DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page,
	META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (dir_meta_ptr) {
		dir_meta_ptr->generation = GENERATION_NUM;
		if (this_inode == INO_CHILDREN_IS_EMPTY)
			dir_meta_ptr->total_children = 0;
		else
			dir_meta_ptr->total_children = 20;

		if (this_inode == INO_DIR)
			dir_meta_ptr->next_xattr_page = 0;
		if (this_inode == INO_DIR_XATTR_PAGE_EXIST)
			dir_meta_ptr->next_xattr_page = sizeof(XATTR_PAGE);
	}

	return 0;
}


int meta_cache_update_dir_data(ino_t this_inode, const struct stat *inode_stat,
    const DIR_META_TYPE *dir_meta_ptr, const DIR_ENTRY_PAGE *dir_page,
    META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (this_inode == INO_META_CACHE_UPDATE_DIR_SUCCESS)
		return 0;
	else if (this_inode == INO_META_CACHE_UPDATE_DIR_FAIL)
		return -1;
	return 0;
}


META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	/* Don't care the return value except NULL. It is not used. */
	return 1; 
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
	if (inode_stat) {
		inode_stat->st_ino = this_inode;
		inode_stat->st_size = NUM_BLOCKS * MOCK_BLOCK_SIZE;
		if (this_inode == INO_REGFILE || this_inode == INO_REGFILE_XATTR_PAGE_EXIST)
			inode_stat->st_mode = S_IFREG;
		else if (this_inode == INO_DIR || this_inode == INO_DIR_XATTR_PAGE_EXIST)
			inode_stat->st_mode = S_IFDIR;
		else
			inode_stat->st_mode = S_IFLNK;
	}

	if(file_meta_ptr) {
		file_meta_ptr->generation = GENERATION_NUM;
		if (this_inode == INO_REGFILE)
			file_meta_ptr->next_xattr_page = 0;
		if (this_inode == INO_REGFILE_XATTR_PAGE_EXIST)
			file_meta_ptr->next_xattr_page = sizeof(XATTR_PAGE);
	}

	return 0;
}

int meta_cache_update_file_data(ino_t this_inode, const struct stat *inode_stat,
    const FILE_META_TYPE *file_meta_ptr, const BLOCK_ENTRY_PAGE *block_page,
    const long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (this_inode == INO_META_CACHE_UPDATE_FILE_SUCCESS)
		return 0;
	else if (this_inode == INO_META_CACHE_UPDATE_FILE_FAIL)
		return -1;
	return 0;
}

int dir_add_entry(ino_t parent_inode, ino_t child_inode, char *childname,
	mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (parent_inode == INO_DIR_ADD_ENTRY_SUCCESS)
		return 0;
	else if (parent_inode == INO_DIR_ADD_ENTRY_FAIL)
		return -1;
	return 0;
}

int dir_remove_entry(ino_t parent_inode, ino_t child_inode, char *childname, 
	mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (parent_inode == INO_DIR_REMOVE_ENTRY_FAIL)
		return -1;
	else if (parent_inode == INO_DIR_REMOVE_ENTRY_SUCCESS)
		return 0;
	return 0;
}

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	strcpy(pathname, MOCK_META_PATH);
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

int write_log(int level, char *format, ...)
{
	return 0;
}

