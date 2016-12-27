#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "mock_param.h"

#include "xattr_ops.h"
#include "meta_mem_cache.h"
#include "params.h"
#include "global.h"
#include "dir_statistics.h"
#include "mount_manager.h"

extern SYSTEM_CONF_STRUCT *system_config;

int32_t meta_cache_lookup_dir_data(ino_t this_inode, HCFS_STAT *inode_stat,
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
		//dir_meta_ptr->upload_seq = MOCK_UPLOAD_SEQ;
	}

	return 0;
}


int32_t meta_cache_update_dir_data(ino_t this_inode, const HCFS_STAT *inode_stat,
    const DIR_META_TYPE *dir_meta_ptr, const DIR_ENTRY_PAGE *dir_page,
    META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (this_inode == INO_META_CACHE_UPDATE_DIR_SUCCESS)
		return 0;
	else if (this_inode == INO_META_CACHE_UPDATE_DIR_FAIL)
		return -1;

	//if (dir_meta_ptr)
	//	CHECK_UPLOAD_SEQ = dir_meta_ptr->upload_seq;

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

int32_t meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	free(target_ptr);
	return 0;
}


int32_t meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (body_ptr->fptr == NULL) {
		body_ptr->fptr = fopen(MOCK_META_PATH, "w+");
		setbuf(body_ptr->fptr, NULL);
	}
	return 0;
}


int32_t meta_cache_close_file(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	if (target_ptr->fptr != NULL) {
		fclose(target_ptr->fptr);
		target_ptr->fptr = NULL;
	}

	return 0;
}


int32_t meta_cache_lookup_file_data(ino_t this_inode, HCFS_STAT *inode_stat,
	FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
		int64_t page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (inode_stat) {
		inode_stat->ino = this_inode;
		inode_stat->nlink = 1;
		inode_stat->size = NUM_BLOCKS * MOCK_BLOCK_SIZE;
		if (this_inode == INO_REGFILE || 
			this_inode == INO_REGFILE_XATTR_PAGE_EXIST) {
			inode_stat->mode = S_IFREG;
		} else if (this_inode == INO_DIR || 
			this_inode == INO_DIR_XATTR_PAGE_EXIST) {
			inode_stat->mode = S_IFDIR;
		} else if (this_inode == INO_FIFO) {
			inode_stat->mode = S_IFIFO;
			inode_stat->size = 0;
		} else {
			inode_stat->mode = S_IFLNK;
		}
	}

	if (file_meta_ptr) {
		memset(file_meta_ptr, 0, sizeof(FILE_META_TYPE));
		file_meta_ptr->generation = GENERATION_NUM;
		if (this_inode == INO_REGFILE)
			file_meta_ptr->next_xattr_page = 0;
		if (this_inode == INO_REGFILE_XATTR_PAGE_EXIST)
			file_meta_ptr->next_xattr_page = sizeof(XATTR_PAGE);

		//file_meta_ptr->upload_seq = MOCK_UPLOAD_SEQ;
	}

	if (this_inode == INO_TOO_MANY_LINKS)
		inode_stat->nlink = MAX_HARD_LINK;

	return 0;
}

int32_t meta_cache_update_file_data(ino_t this_inode, const HCFS_STAT *inode_stat,
    const FILE_META_TYPE *file_meta_ptr, const BLOCK_ENTRY_PAGE *block_page,
    const int64_t page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (this_inode == INO_META_CACHE_UPDATE_FILE_SUCCESS)
		return 0;
	else if (this_inode == INO_META_CACHE_UPDATE_FILE_FAIL)
		return -1;

	//if (file_meta_ptr)
	//	CHECK_UPLOAD_SEQ = file_meta_ptr->upload_seq;

	return 0;
}

int32_t dir_add_entry(ino_t parent_inode, ino_t child_inode, char *childname,
	mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr, BOOL is_external)
{
	if (parent_inode == INO_DIR_ADD_ENTRY_SUCCESS)
		return 0;
	else if (parent_inode == INO_DIR_ADD_ENTRY_FAIL)
		return -1;
	return 0;
}

int32_t dir_remove_entry(ino_t parent_inode, ino_t child_inode, char *childname, 
	mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr, BOOL is_external)
{
	if (parent_inode == INO_DIR_REMOVE_ENTRY_FAIL)
		return -1;
	else if (parent_inode == INO_DIR_REMOVE_ENTRY_SUCCESS)
		return 0;
	return 0;
}

int32_t fetch_meta_path(char *pathname, ino_t this_inode)
{
	strcpy(pathname, MOCK_META_PATH);
	return 0;
}

int32_t init_dir_page(DIR_ENTRY_PAGE *tpage, ino_t self_inode, ino_t parent_inode, 
	int64_t this_page_pos)
{
	return 0;
}

int32_t decrease_nlink_inode_file(ino_t this_inode)
{
	return 0;
}

int32_t mark_inode_delete(ino_t this_inode)
{
	return 0;
}

int32_t write_log(int32_t level, const char *format, ...)
{
	return 0;
}

int32_t flush_single_entry(META_CACHE_ENTRY_STRUCT *meta_cache_entry)
{
	return 0;
}

int32_t meta_cache_update_symlink_data(ino_t this_inode, 
	const HCFS_STAT *inode_stat, 
	const SYMLINK_META_TYPE *symlink_meta_ptr, 
	META_CACHE_ENTRY_STRUCT *bptr)
{
	char buf[5000];

	memset(buf, 0, 5000);

	if (symlink_meta_ptr) {
		if (symlink_meta_ptr->link_path)
			memcpy(buf, symlink_meta_ptr->link_path,
					symlink_meta_ptr->link_len);

		if (!strcmp("update_symlink_data_fail", buf))
			return -1;

		//CHECK_UPLOAD_SEQ = symlink_meta_ptr->upload_seq;
	}
	return 0;
}

int32_t meta_cache_lookup_symlink_data(ino_t this_inode, HCFS_STAT *inode_stat,
        SYMLINK_META_TYPE *symlink_meta_ptr, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (symlink_meta_ptr) {
		memset(symlink_meta_ptr, 0, sizeof(SYMLINK_META_TYPE));
		symlink_meta_ptr->generation = GENERATION_NUM;

		if (this_inode == INO_LNK)
			symlink_meta_ptr->next_xattr_page = 0;
		if (this_inode == INO_LNK_XATTR_PAGE_EXIST)
			symlink_meta_ptr->next_xattr_page = sizeof(XATTR_PAGE);
		//symlink_meta_ptr->upload_seq = MOCK_UPLOAD_SEQ;
	}
	return 0;
}
int32_t pathlookup_write_parent(ino_t self_inode, ino_t parent_inode)
{
	return 0;
}

int32_t change_pin_flag(ino_t this_inode, mode_t this_mode, char new_pin_status)
{
	if (this_inode == 1) /* Case of failure */
		return -ENOMEM;
	if (this_inode == 2)
		return 1; /* Case of the same flag as old one */
	return 0;
}

int32_t collect_dir_children(ino_t this_inode, ino_t **dir_node_list,
	int64_t *num_dir_node, ino_t **nondir_node_list,
	int64_t *num_nondir_node)
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

int32_t super_block_mark_pin(ino_t this_inode, mode_t this_mode)
{
	return 0;
}

int32_t super_block_mark_unpin(ino_t this_inode, mode_t this_mode)
{
	return 0;
}

int32_t reset_dirstat_lookup(ino_t thisinode)
{
	return 0;
}

int32_t update_dirstat_parent(ino_t baseinode, DIR_STATS_TYPE *newstat)
{
	return 0;
}
int32_t check_file_storage_location(FILE *fptr,  DIR_STATS_TYPE *newstat)
{
	return 0;
}
int32_t lookup_add_parent(ino_t self_inode, ino_t parent_inode)
{
	return 0;
}
int32_t lookup_delete_parent(ino_t self_inode, ino_t parent_inode)
{
	return 0;
}

int update_meta_seq(META_CACHE_ENTRY_STRUCT *bptr)
{
	return 0;
}

int32_t inherit_xattr(ino_t parent_inode, ino_t this_inode,
		META_CACHE_ENTRY_STRUCT *selbody_ptr)
{
	return 0;
}

int fetch_toupload_meta_path(char *pathname, ino_t inode)
{
	return 0;
}

int32_t get_meta_size(ino_t inode, int64_t *metasize, int64_t *metaroundsize)
{
	return 0;
}

int check_and_copy_file(const char *srcpath, const char *tarpath, BOOL lock_src)
{
	return 0;
}

int meta_cache_sync_later(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int32_t meta_cache_remove_sync_later(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int32_t meta_cache_set_uploading_info(META_CACHE_ENTRY_STRUCT *body_ptr,
		BOOL is_now_uploading, int32_t new_fd, int64_t toupload_blocks)
{
	CHECK_UPLOADING_FLAG = is_now_uploading;
	CHECK_TOUPLOAD_BLOCKS = toupload_blocks;
	return 0;
}

int32_t meta_cache_get_meta_size(META_CACHE_ENTRY_STRUCT *ptr, int64_t *metasize, int64_t *metaroundsize)
{
	int32_t ret;
	int32_t errcode;
	int64_t ret_pos;

	*metasize = 0;
	assert(ptr != NULL);
	assert(ptr->fptr != NULL);
	LSEEK(fileno(ptr->fptr), 0, SEEK_END);
	*metasize = ret_pos;
	return 0;

errcode_handle:
	return errcode;
	return 0;
}

int64_t get_pinned_limit(const char pin_type)
{
	if (pin_type < NUM_PIN_TYPES)
		return PINNED_LIMITS(pin_type);
	else
		return -EINVAL;
}


int32_t change_system_meta(int64_t system_data_size_delta,
		int64_t meta_size_delta, int64_t cache_data_size_delta,
		int64_t cache_blocks_delta, int64_t dirty_cache_delta,
		int64_t unpin_dirty_delta, BOOL need_sync)
{
	return 0;
}

int32_t change_mount_stat(MOUNT_T *mptr, int64_t system_size_delta,
		int64_t meta_size_delta, int64_t num_inodes_delta)
{
	return 0;
}

int64_t round_size(int64_t size)
{
	int64_t blksize = 4096;
	int64_t ret_size;

	if (size >= 0) {
		/* round up to filesystem block size */
		ret_size = (size + blksize - 1) & (~(blksize - 1));
	} else {
		size = -size;
		ret_size = -((size + blksize - 1) & (~(blksize - 1)));
	}

	return ret_size;
}

