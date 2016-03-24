#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>

#include "mock_param.h"

#include "xattr_ops.h"
#include "meta_mem_cache.h"
#include "params.h"
#include "global.h"
#include "dir_statistics.h"

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
		dir_meta_ptr->local_pin = 1;
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
	META_CACHE_ENTRY_STRUCT *tmpptr;

	tmpptr = (META_CACHE_ENTRY_STRUCT *)
			malloc(sizeof(META_CACHE_ENTRY_STRUCT));
	tmpptr->fptr = NULL;
	tmpptr->meta_opened = FALSE;
	return tmpptr;
}

int meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	free(target_ptr);
	return 0;
}


int meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (body_ptr->fptr == NULL)
		body_ptr->fptr = fopen(MOCK_META_PATH, "w+");
	return 0;
}


int meta_cache_close_file(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	if (target_ptr->fptr != NULL) {
		fclose(target_ptr->fptr);
		target_ptr->fptr = NULL;
	}

	return 0;
}


int meta_cache_lookup_file_data(ino_t this_inode, struct stat *inode_stat,
	FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
		long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (inode_stat) {
		inode_stat->st_ino = this_inode;
		inode_stat->st_nlink = 1;
		inode_stat->st_size = NUM_BLOCKS * MOCK_BLOCK_SIZE;
		if (this_inode == INO_REGFILE || 
			this_inode == INO_REGFILE_XATTR_PAGE_EXIST) {
			inode_stat->st_mode = S_IFREG;
		} else if (this_inode == INO_DIR || 
			this_inode == INO_DIR_XATTR_PAGE_EXIST) {
			inode_stat->st_mode = S_IFDIR;
		} else if (this_inode == INO_FIFO) {
			inode_stat->st_mode = S_IFIFO;
			inode_stat->st_size = 0;
		} else {
			inode_stat->st_mode = S_IFLNK;
		}
	}

	if (file_meta_ptr) {
		file_meta_ptr->generation = GENERATION_NUM;
		if (this_inode == INO_REGFILE)
			file_meta_ptr->next_xattr_page = 0;
		if (this_inode == INO_REGFILE_XATTR_PAGE_EXIST)
			file_meta_ptr->next_xattr_page = sizeof(XATTR_PAGE);
	}

	if (this_inode == INO_TOO_MANY_LINKS)
		inode_stat->st_nlink = MAX_HARD_LINK;

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

int flush_single_entry(META_CACHE_ENTRY_STRUCT *meta_cache_entry)
{
	return 0;
}

int meta_cache_update_symlink_data(ino_t this_inode, 
	const struct stat *inode_stat, 
	const SYMLINK_META_TYPE *symlink_meta_ptr, 
	META_CACHE_ENTRY_STRUCT *bptr)
{
	char buf[5000];

	memset(buf, 0, 5000);
	memcpy(buf, symlink_meta_ptr->link_path, symlink_meta_ptr->link_len);

	if (!strcmp("update_symlink_data_fail", buf))
		return -1;

	return 0;
}

int meta_cache_lookup_symlink_data(ino_t this_inode, struct stat *inode_stat,
        SYMLINK_META_TYPE *symlink_meta_ptr, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (symlink_meta_ptr) {
		symlink_meta_ptr->generation = GENERATION_NUM;

		if (this_inode == INO_LNK)
			symlink_meta_ptr->next_xattr_page = 0;
		if (this_inode == INO_LNK_XATTR_PAGE_EXIST)
			symlink_meta_ptr->next_xattr_page = sizeof(XATTR_PAGE);
	}
	return 0;
}
int pathlookup_write_parent(ino_t self_inode, ino_t parent_inode)
{
	return 0;
}

int change_pin_flag(ino_t this_inode, mode_t this_mode, char new_pin_status)
{
	if (this_inode == 1) /* Case of failure */
		return -ENOMEM;
	if (this_inode == 2)
		return 1; /* Case of the same flag as old one */
	return 0;
}

int collect_dir_children(ino_t this_inode, ino_t **dir_node_list,
	long long *num_dir_node, ino_t **nondir_node_list,
	long long *num_nondir_node)
{
	*num_dir_node = 0;
	*num_nondir_node = 0;
	*dir_node_list = NULL;
	*nondir_node_list = NULL;
	if (collect_dir_children_flag == TRUE) {
		*dir_node_list = malloc(sizeof(ino_t));
		*num_dir_node = 1;
		(*dir_node_list)[0] = INO_LNK; /* Directly return 0 at next layer */

		*nondir_node_list = malloc(sizeof(ino_t));
		*num_nondir_node = 1;
		(*nondir_node_list)[0] = INO_LNK; /* Directly return 0 at next layer */
	}

	return 0;
}

int super_block_mark_pin(ino_t this_inode, mode_t this_mode)
{
	return 0;
}

int super_block_mark_unpin(ino_t this_inode, mode_t this_mode)
{
	return 0;
}

int reset_dirstat_lookup(ino_t thisinode)
{
	return 0;
}

int update_dirstat_parent(ino_t baseinode, DIR_STATS_TYPE *newstat)
{
	return 0;
}
int check_file_storage_location(FILE *fptr,  DIR_STATS_TYPE *newstat)
{
	return 0;
}
int lookup_add_parent(ino_t self_inode, ino_t parent_inode)
{
	return 0;
}
int lookup_delete_parent(ino_t self_inode, ino_t parent_inode)
{
	return 0;
}

int inherit_xattr(ino_t parent_inode, ino_t this_inode,
		META_CACHE_ENTRY_STRUCT *selbody_ptr)
{
	return 0;
}

int get_meta_size(ino_t inode, long long *metasize)
{
	return 0;
}
