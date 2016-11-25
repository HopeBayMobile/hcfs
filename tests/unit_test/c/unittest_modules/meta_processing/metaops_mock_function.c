#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <fuse/fuse_lowlevel.h>
#include <inttypes.h>
#include <string.h>

#include "mock_param.h"

#include "meta_mem_cache.h"
#include "params.h"
#include "mount_manager.h"
#include "xattr_ops.h"
#include "global.h"
#include "do_restoration.h"

/* Global vars*/
int32_t DELETE_DIR_ENTRY_BTREE_RESULT = 1;

/* mock functions - meta_mem_cache*/
int32_t meta_cache_lookup_dir_data(ino_t this_inode, HCFS_STAT *inode_stat,
	DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page,
	META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (dir_meta_ptr)
		dir_meta_ptr->local_pin = pin_flag_in_meta;

	switch(this_inode) {
	case INO_LOOKUP_DIR_DATA_OK_WITH_STLINK_2:
		if (inode_stat != NULL) {
			inode_stat->nlink = this_inode + 1;
			inode_stat->size = 0;
		}
		return 0;
	case INO_LOOKUP_DIR_DATA_OK_WITH_BlocksToDel:
		if (inode_stat != NULL) {
			inode_stat->nlink = 1;
			inode_stat->size = MOCK_BLOCK_SIZE * NUM_BLOCKS;
		}
		return 0;
	case INO_LOOKUP_DIR_DATA_OK_WITH_NoBlocksToDel:
		if (inode_stat != NULL) {
			inode_stat->nlink = 1;
			inode_stat->size = 0;
		}
		return 0;
	case INO_NO_XATTR_PAGE:
		if (dir_meta_ptr) {
			dir_meta_ptr->next_xattr_page = 0;
		}
		return 0;
	case INO_XATTR_PAGE_EXIST:
		if (dir_meta_ptr) {
			dir_meta_ptr->next_xattr_page = 123;
		}
		return 0;
	default:
		if (!dir_meta_ptr)
			return 0;
		dir_meta_ptr->root_entry_page = 0; 
		dir_meta_ptr->entry_page_gc_list = 0;
		dir_meta_ptr->tree_walk_list_head = sizeof(DIR_ENTRY_PAGE);
		dir_meta_ptr->total_children = to_verified_meta.total_children;
		if (inode_stat)
			inode_stat->nlink = to_verified_stat.nlink;
		return 0;
	}
}

int32_t meta_cache_update_dir_data(ino_t this_inode, const HCFS_STAT *inode_stat,
    const DIR_META_TYPE *dir_meta_ptr, const DIR_ENTRY_PAGE *dir_page,
    META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (dir_meta_ptr)
		to_verified_meta = *dir_meta_ptr;
	
	if (inode_stat)
		to_verified_stat = *inode_stat;
	
	return 0;
}

int32_t meta_cache_seek_dir_entry(ino_t this_inode, DIR_ENTRY_PAGE *result_page,
	int32_t *result_index, const char *childname, META_CACHE_ENTRY_STRUCT *body_ptr,
	BOOL is_external)
{
	switch(this_inode) {
        case INO_SEEK_DIR_ENTRY_OK:
		*result_index = 0;
		return 0;
        case INO_SEEK_DIR_ENTRY_NOTFOUND:
		*result_index = -1;
		return 0;
	case INO_SEEK_DIR_ENTRY_FAIL:
		return -1;
	default:
		return 0;
	}
}


META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	META_CACHE_ENTRY_STRUCT *bptr;

	if (this_inode != INO_LOOKUP_FILE_DATA_OK_LOCK_ENTRY_FAIL) {
		bptr = (META_CACHE_ENTRY_STRUCT *)
				malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		memset(bptr, 0, sizeof(META_CACHE_ENTRY_STRUCT));
		return bptr;
	} else {
		errno = ENOMEM;
		return NULL;
	}

	errno = EINVAL;
	return NULL;
}


int32_t meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	free(target_ptr);
	return 0;
}


int32_t meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (test_change_pin_flag) {
		body_ptr->fptr = fopen("test_meta_file", "r");
		setbuf(body_ptr->fptr, NULL);
		if (body_ptr->fptr == NULL)
			return errno;
	}
	return 0;
}


int32_t meta_cache_close_file(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	if (test_change_pin_flag && target_ptr->fptr) {
		printf("fileno %d\n", fileno(target_ptr->fptr));
		//fclose(target_ptr->fptr);
		target_ptr->fptr = NULL;
	}
	return 0;
}


int32_t meta_cache_drop_pages(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}


int32_t meta_cache_remove(ino_t this_inode)
{

	char metapath[METAPATHLEN];

	/* Remove todel file here */
	sprintf(metapath, "testpatterns/inode_%" PRIu64 "_meta_file.todel", (uint64_t)this_inode);
	if (!access(metapath, F_OK))
		unlink(metapath);

	return 0;
}


int32_t meta_cache_lookup_file_data(ino_t this_inode, HCFS_STAT *inode_stat,
	FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
		int64_t page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{

	file_meta_ptr->local_pin = pin_flag_in_meta;
	switch(this_inode) {
       	case INO_DIRECT_SUCCESS:
		file_meta_ptr->direct = sizeof(FILE_META_TYPE);
		return 0;
	case INO_SINGLE_INDIRECT_SUCCESS:
		file_meta_ptr->single_indirect = sizeof(FILE_META_TYPE);
		return 0;

        case INO_DOUBLE_INDIRECT_SUCCESS:
		file_meta_ptr->double_indirect = sizeof(FILE_META_TYPE);
		return 0;

	case INO_TRIPLE_INDIRECT_SUCCESS:
		file_meta_ptr->triple_indirect = sizeof(FILE_META_TYPE);
		return 0;

	case INO_QUADRUPLE_INDIRECT_SUCCESS:
		file_meta_ptr->quadruple_indirect = sizeof(FILE_META_TYPE);
		return 0;
        case INO_CREATE_PAGE_SUCCESS:
		memset(file_meta_ptr, 0, sizeof(FILE_META_TYPE));
		return 0;
	default:
		return 0;
	}
}

int32_t meta_cache_update_file_data(ino_t this_inode, const HCFS_STAT *inode_stat,
    const FILE_META_TYPE *file_meta_ptr, const BLOCK_ENTRY_PAGE *block_page,
    const int64_t page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	switch(this_inode) {
        case INO_UPDATE_FILE_DATA_FAIL:
		return -1;
	case INO_CREATE_PAGE_SUCCESS:
		if (!body_ptr->fptr)
			return 0;
		if (file_meta_ptr)
			pwrite(fileno(body_ptr->fptr), file_meta_ptr, 
				sizeof(FILE_META_TYPE), 0);
		if (block_page)
			pwrite(fileno(body_ptr->fptr), block_page, 
				sizeof(BLOCK_ENTRY_PAGE), page_pos);
	default:
		return 0;
	}
}


/* mock functions - dir_entry_btree*/
int32_t insert_dir_entry_btree(DIR_ENTRY *new_entry, DIR_ENTRY_PAGE *tnode,
	int32_t fh, DIR_ENTRY *overflow_median, int64_t *overflow_new_page,
	DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
						int64_t *temp_child_page_pos)
{
	DIR_ENTRY_PAGE tmp_page;
	
	*overflow_new_page = sizeof(DIR_ENTRY_PAGE);
	pwrite(fh, &tmp_page, sizeof(DIR_ENTRY_PAGE), sizeof(DIR_ENTRY_PAGE));
	
	switch (new_entry->d_ino) {
	case INO_INSERT_DIR_ENTRY_FAIL:
		return -1;
	case INO_INSERT_DIR_ENTRY_SUCCESS_WITHOUT_SPLITTING:
		return 0;
	case INO_INSERT_DIR_ENTRY_SUCCESS_WITH_SPLITTING:
		return 1;
	}
	return 0;
}


int32_t delete_dir_entry_btree(DIR_ENTRY *to_delete_entry, DIR_ENTRY_PAGE *tnode,
		int32_t fh, DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
		int64_t *temp_child_page_pos)
{

	if (DELETE_DIR_ENTRY_BTREE_RESULT) {
		return 0;
	} else {
		return -1;
	}
}


/* mock functions - super_block.c*/
int32_t super_block_to_delete(ino_t this_inode)
{
	/*if (this_inode == INO_DELETE_DIR)
		return -1;
	if (this_inode == INO_DELETE_LNK)
		return -2;*/
	return 0;
}


/* mock functions - hfuse_system.c*/
int32_t sync_hcfs_system_data(char need_lock)
{
	return 0;
}


/* mock functions - utils.c*/
off_t check_file_size(const char *path)
{
	return round_size(MAX_BLOCK_SIZE);
}


int32_t fetch_todelete_path(char *pathname, ino_t this_inode)
{
	//sprintf(pathname, "testpatterns/inode_%" PRIu64 "_meta_file.todel",
	//(uint64_t)this_inode);
	strcpy(pathname, TO_DELETE_METAPATH);
	if (this_inode == INO_RENAME_FAIL) {
		strcpy(pathname, "\0");
		/*FILE *fptr = fopen(pathname, "w");
		fclose(fptr);
		HCFS_STAT filestat;
		stat(pathname, &filestat);
		mode_t mode = filestat.st_mode | S_ISVTX;
		chmod(pathname, mode);*/
	}
	return 0;
}


int32_t fetch_meta_path(char *pathname, ino_t this_inode)
{
	sprintf(pathname, "%s_%" PRIu64 "", MOCK_META_PATH, (uint64_t)this_inode);
	//strcpy(pathname, MOCK_META_PATH);
	return 0;
}


int32_t fetch_block_path(char *pathname, ino_t this_inode, int64_t block_num)
{
	sprintf(pathname, "testpatterns/inode_%" PRIu64 "_block_%"PRId64,
			(uint64_t)this_inode, block_num);
	return 0;
}

int32_t fetch_inode_stat(ino_t this_inode, HCFS_STAT *inode_stat, 
	uint64_t *ret_gen)
{
	if (this_inode == INO_DELETE_FILE_BLOCK) {
		inode_stat->size = NUM_BLOCKS * MAX_BLOCK_SIZE;
	} else {
		inode_stat->size = 0;
		inode_stat->mode = (this_inode % 2 ? S_IFLNK : S_IFDIR);
	}
	return 0;
}

int32_t lookup_markdelete(LOOKUP_HEAD_TYPE *lookup_table, ino_t this_inode)
{
	return 0;
}

void set_timestamp_now(HCFS_STAT *thisstat, char mode)
{
	return;
}

int32_t write_log(int32_t level, const char *format, ...)
{
	va_list alist;

	if (level > 8)
		return 0;
	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}
MOUNT_T tmpmount;

void* fuse_req_userdata(fuse_req_t req)
{
	tmpmount.f_ino = ROOT_INODE;
	return &tmpmount;
}

int32_t change_mount_stat(MOUNT_T *mptr, int64_t system_size_delta, 
	int64_t meta_size_delta, int64_t num_inodes_delta)
{
	return 0;
}

int32_t flush_single_entry(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}
int32_t fetch_stat_path(char *pathname, ino_t this_inode)
{
        snprintf(pathname, 100, "%s/stat%ld", METAPATH, this_inode);
        return 0;
}

int32_t pathlookup_write_parent(ino_t self_inode, ino_t parent_inode)
{
	if (pathlookup_write_parent_success == TRUE)
		return 0;
	else
		return -EIO;
}

int32_t delete_pathcache_node(PATH_CACHE *cacheptr, ino_t todelete)
{
	if (delete_pathcache_node_success == TRUE)
		return 0;
	else
		return -EINVAL;
}

int32_t meta_cache_update_symlink_data(ino_t this_inode, const HCFS_STAT *inode_stat,
        const SYMLINK_META_TYPE *symlink_meta_ptr, META_CACHE_ENTRY_STRUCT *bptr)
{
	return 0;
}

int32_t meta_cache_lookup_symlink_data(ino_t this_inode, HCFS_STAT *inode_stat,
        SYMLINK_META_TYPE *symlink_meta_ptr, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (symlink_meta_ptr)
		symlink_meta_ptr->local_pin = pin_flag_in_meta;
	return 0;
}

int32_t parse_xattr_namespace(const char *name, char *name_space, char *key)
{
	strcpy(key, name);
	*name_space = global_mock_namespace;

	return 0;
}

int32_t insert_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, const int64_t xattr_filepos,
	const char name_space, const char *key,
	const char *value, const size_t size, const int32_t flag)
{
	printf("key is %s, value size is %ld\n", key, size);
	if (xattr_count == 3)
		xattr_count = 0;
	strcpy(xattr_key[xattr_count++], key); /* To be verified */
	return 0;
}

int32_t get_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page,
	const char name_space, const char *key, char *value, const size_t size,
	size_t *actual_size)
{
	*actual_size = XATTR_VALUE_SIZE;

	if (size < XATTR_VALUE_SIZE) {
		return -ERANGE;
	}

	return 0;
}

int32_t list_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, char *key_buf, const size_t size,
	size_t *actual_size)
{
	if (size == 0) {
		*actual_size = TOTAL_KEY_SIZE;
	} else {
		if (size < TOTAL_KEY_SIZE)
			return -ERANGE;
		strcpy(key_buf, "key1");
		strcpy(key_buf + 5, "key2");
		strcpy(key_buf + 10, "key3");
		*actual_size = TOTAL_KEY_SIZE;
	}
	return 0;
}

int32_t fetch_xattr_page(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
		XATTR_PAGE *xattr_page, int64_t *xattr_pos)
{
        return 0;
}
int32_t construct_path(PATH_CACHE *cacheptr, ino_t thisinode, char **result,
                   ino_t rootinode)
{
	*result = (char *) malloc(50);
	snprintf(*result, 50, "/tmp/markdeletetmp");
	return 0;
}

int32_t change_system_meta(int64_t system_size_delta, int64_t meta_size_delta,
		int64_t cache_size_delta, int64_t cache_blocks_delta,
		int64_t dirty_cache_delta, int64_t unpin_dirty_data_size,
		BOOL need_sync)
{
	hcfs_system->systemdata.cache_size += (cache_size_delta);
	hcfs_system->systemdata.cache_blocks += cache_blocks_delta;
	hcfs_system->systemdata.dirty_cache_size += dirty_cache_delta;
	hcfs_system->systemdata.unpin_dirty_data_size += unpin_dirty_data_size;
	hcfs_system->systemdata.system_size += system_size_delta;
	hcfs_system->systemdata.system_meta_size += meta_size_delta;
	return 0;
}

int32_t handle_dirmeta_snapshot(ino_t thisinode, FILE *metafptr)
{
	return 0;
}
int32_t meta_nospc_log(const char *func_name, int32_t lines)
{
	return 1;
}

int32_t super_block_reclaim(void)
{
	return 0;
}

int32_t super_block_delete(ino_t this_inode)
{
	return 0;
}

int32_t super_block_enqueue_delete(ino_t this_inode)
{
	return 0;
}

int32_t change_pin_size(int64_t delta_pin_size)
{
	return 0;
}

void fetch_temp_restored_meta_path(char *pathname, ino_t this_inode)
{
	sprintf(pathname, "restore_meta_fileTestPath/restore_meta_%"PRIu64,
			(uint64_t)this_inode);
	return;
}
int32_t fetch_from_cloud(FILE *fptr, char action_from, char *objname)
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

void update_rectified_system_meta(DELTA_SYSTEM_META delta_system_meta)
{
	SYSTEM_DATA_TYPE *rectified_system_meta;

	rectified_system_meta =
			&(hcfs_restored_system_meta->rectified_system_meta);

	/* Update rectified space usage */
	rectified_system_meta->system_size +=
			delta_system_meta.delta_system_size;
	rectified_system_meta->system_meta_size +=
			delta_system_meta.delta_meta_size;
	rectified_system_meta->pinned_size +=
			delta_system_meta.delta_pinned_size;
	rectified_system_meta->backend_size +=
			delta_system_meta.delta_backend_size;
	rectified_system_meta->backend_meta_size +=
			delta_system_meta.delta_backend_meta_size;
	rectified_system_meta->backend_inodes +=
			delta_system_meta.delta_backend_inodes;
	return;
}

void fetch_progress_file_path(char *pathname, ino_t inode)
{
	sprintf(pathname, "testpatterns/mock_progress_file");
	return;
}

int32_t fetch_restore_block_path(char *pathname,
		ino_t this_inode, int64_t block_num)
{
	return 0;
}

int32_t copy_file(const char *srcpath, const char *tarpath)
{
	return 0;
}

int32_t update_restored_cache_usage(int64_t delta_cache_size,
				 int64_t delta_cache_blocks,
				 char pin_type)
{
	return 0;
}

