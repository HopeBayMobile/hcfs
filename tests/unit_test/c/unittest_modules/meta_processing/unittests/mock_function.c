#include <sys/stat.h>
#include <stdio.h>

#include "mock_param.h"

#include "meta_mem_cache.h"
#include "params.h"

/* Global vars*/
int DELETE_DIR_ENTRY_BTREE_RESULT = 1;

SYSTEM_CONF_STRUCT system_config;

/* mock functions - meta_mem_cache*/
int meta_cache_lookup_dir_data(ino_t this_inode, struct stat *inode_stat,
	DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page,
	META_CACHE_ENTRY_STRUCT *body_ptr)
{

	switch(this_inode) {
	case INO_LOOKUP_DIR_DATA_OK_WITH_STLINK_2:
		if (inode_stat != NULL) {
			inode_stat->st_nlink = this_inode + 1;
			inode_stat->st_size = 0;
		}
		return 0;
	case INO_LOOKUP_DIR_DATA_OK_WITH_BlocksToDel:
		if (inode_stat != NULL) {
			inode_stat->st_nlink = 1;
			inode_stat->st_size = MOCK_BLOCK_SIZE * NUM_BLOCKS;
		}
		return 0;
	case INO_LOOKUP_DIR_DATA_OK_WITH_NoBlocksToDel:
		if (inode_stat != NULL) {
			inode_stat->st_nlink = 1;
			inode_stat->st_size = 0;
		}
		return 0;
	default:
		dir_meta_ptr->root_entry_page = 0; 
		dir_meta_ptr->total_children = to_verified_meta.total_children;
		inode_stat->st_nlink = to_verified_stat.st_nlink;
		return 0;
	}
}

int meta_cache_update_dir_data(ino_t this_inode, const struct stat *inode_stat,
    const DIR_META_TYPE *dir_meta_ptr, const DIR_ENTRY_PAGE *dir_page,
    META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (dir_meta_ptr)
		to_verified_meta = *dir_meta_ptr;
	
	if (dir_meta_ptr)
		to_verified_stat = *inode_stat;
	
	return 0;
}

int meta_cache_seek_dir_entry(ino_t this_inode, DIR_ENTRY_PAGE *result_page,
	int *result_index, char *childname, META_CACHE_ENTRY_STRUCT *body_ptr)
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


int meta_cache_drop_pages(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}


int meta_cache_remove(ino_t this_inode)
{

	char metapath[METAPATHLEN];

	/* Remove todel file here */
	sprintf(metapath, "testpatterns/inode_%d_meta_file.todel", this_inode);
	unlink(metapath);

	return 0;
}


int meta_cache_lookup_file_data(ino_t this_inode, struct stat *inode_stat,
	FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
		long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{

	switch(this_inode) {
        case INO_LOOKUP_FILE_DATA_OK:
        case INO_UPDATE_FILE_DATA_FAIL:
		if (file_meta_ptr != NULL) {
			//file_meta_ptr->next_block_page = sizeof(BLOCK_ENTRY_PAGE);

			BLOCK_ENTRY_PAGE *tmp_page = (BLOCK_ENTRY_PAGE*)malloc(sizeof(BLOCK_ENTRY_PAGE));
			fwrite(tmp_page, sizeof(BLOCK_ENTRY_PAGE), 1, body_ptr->fptr);
			free(tmp_page);
		}

		if (block_page != NULL) {
			/* Assume there are NUM_BLOCKS blocks */
			//if (page_pos >= sizeof(BLOCK_ENTRY_PAGE)*NUM_BLOCKS) {
			//	block_page->next_page = 0;
			//} else {
			//	block_page->next_page = page_pos + sizeof(BLOCK_ENTRY_PAGE);
			//}
			fwrite(block_page, sizeof(BLOCK_ENTRY_PAGE), 1, body_ptr->fptr);
		}
		
		return 0;
        case INO_LOOKUP_FILE_DATA_OK_NO_BLOCK_PAGE:
		//file_meta_ptr->next_block_page = 0;
		return 0;
	case INO_LOOKUP_FILE_DATA_FAIL:
		return -1;
	default:
		return 0;
	}
}

int meta_cache_update_file_data(ino_t this_inode, const struct stat *inode_stat,
    const FILE_META_TYPE *file_meta_ptr, const BLOCK_ENTRY_PAGE *block_page,
    const long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	switch(this_inode) {
        case INO_UPDATE_FILE_DATA_FAIL:
		return -1;
	default:
		return 0;
	}
}


/* mock functions - dir_entry_btree*/
int insert_dir_entry_btree(DIR_ENTRY *new_entry, DIR_ENTRY_PAGE *tnode,
	int fh, DIR_ENTRY *overflow_median, long long *overflow_new_page,
	DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
						long long *temp_child_page_pos)
{
	return 0;
}


int delete_dir_entry_btree(DIR_ENTRY *to_delete_entry, DIR_ENTRY_PAGE *tnode,
		int fh, DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
		long long *temp_child_page_pos)
{

	if (DELETE_DIR_ENTRY_BTREE_RESULT) {
		return 0;
	} else {
		return -1;
	}
}


/* mock functions - super_block.c*/
int super_block_to_delete(ino_t this_inode)
{
	return 0;
}


/* mock functions - hfuse_system.c*/
int sync_hcfs_system_data(char need_lock)
{
	return 0;
}


/* mock functions - utils.c*/
off_t check_file_size(const char *path)
{
	return MOCK_BLOCK_SIZE;
}


int fetch_todelete_path(char *pathname, ino_t this_inode)
{
	sprintf(pathname, "testpatterns/inode_%d_meta_file.todel", this_inode);
	return 0;
}


int fetch_meta_path(char *pathname, ino_t this_inode)
{
	sprintf(pathname, "testpatterns/inode_%d_meta_file", this_inode);
	return 0;
}


int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	sprintf(pathname, "testpatterns/inode_%d_block_%d", this_inode, block_num);
	return 0;
}
