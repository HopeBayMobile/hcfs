#include <sys/stat.h>
#include <semaphore.h>
#include <stdio.h>
#include "super_block.h"
#include "fuseop.h"
#include "mock_tool.h"
#include "meta_mem_cache.h"
/*
	Mock fetch_meta_path() function
 */
int fetch_meta_path(char *path, ino_t inode)
{
	switch(inode){
	case INO__FETCH_META_PATH_FAIL:
		return -1;
	case INO__FETCH_META_PATH_SUCCESS:
		strcpy(path, TMP_META_FILE_PATH);
		return 0;
	case INO__FETCH_META_PATH_ERR:
		return 0;
	default:
		return -1;	
	}
}


int super_block_mark_dirty(ino_t this_inode)
{
	return 0;
}

int super_block_update_stat(ino_t this_inode, struct stat *newstat)
{
	return 0;
}


int dentry_binary_search(DIR_ENTRY *entry_array, int num_entries, DIR_ENTRY *new_entry, int *index_to_insert)
{
	return 0;
}

int search_dir_entry_btree(char *target_name, DIR_ENTRY_PAGE *current_node, int fptr, int *result_index, DIR_ENTRY_PAGE *result_node)
{
	return 0;
}

struct stat *get_test_stat(ino_t inode_num)
{
	struct stat *test_stat = (struct stat *)malloc(sizeof(struct stat));
	memset(test_stat, 0, sizeof(struct stat));
	test_stat->st_ino = inode_num;
	test_stat->st_nlink = inode_num + 3;
	test_stat->st_uid = inode_num + 6;
	test_stat->st_gid = inode_num + 9;
	test_stat->st_size = inode_num * 97;
	return test_stat;

}

int super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	if(inode_ptr == NULL)
		inode_ptr = (SUPER_BLOCK_ENTRY *)malloc(sizeof(SUPER_BLOCK_ENTRY));

	memcpy(&(inode_ptr->inode_stat), get_test_stat(this_inode), sizeof(struct stat));
	return 0;
}




