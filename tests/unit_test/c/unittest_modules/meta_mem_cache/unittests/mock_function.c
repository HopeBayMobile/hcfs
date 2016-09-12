#include <sys/stat.h>
#include <semaphore.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "super_block.h"
#include "fuseop.h"
#include "mock_function.h"
#include "meta_mem_cache.h"
/*
	Mock fetch_meta_path() function
 */
int32_t fetch_meta_path(char *path, ino_t inode)
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


int32_t super_block_mark_dirty(ino_t this_inode)
{
	return 0;
}

int32_t super_block_update_stat(ino_t this_inode,
				HCFS_STAT *newstat,
				BOOL no_sync)
{
	return 0;
}


int32_t dentry_binary_search(DIR_ENTRY *entry_array, int32_t num_entries, DIR_ENTRY *new_entry, int32_t *index_to_insert,
			     BOOL is_external)
{
	int32_t i;
	for (i=0 ; i<num_entries ; i++) {
		if (strcmp(new_entry->d_name, entry_array[i].d_name) == 0) {
			memcpy(new_entry, &entry_array[i], sizeof(DIR_ENTRY));
			return i;
		}
	}
	return -1;
}

int32_t search_dir_entry_btree(char *target_name, DIR_ENTRY_PAGE *tnode, int32_t fh, int32_t *result_index,
			       DIR_ENTRY_PAGE *result_node, BOOL is_external)
{
	DIR_ENTRY tmp_entry;
	int32_t tmp_index;
	int32_t ret;

	strcpy(tmp_entry.d_name, target_name);
	ret = dentry_binary_search(tnode->dir_entries, tnode->num_entries,
		&tmp_entry, &tmp_index, is_external);
	if (ret >= 0) {
		memcpy(result_node, tnode, sizeof(DIR_ENTRY_PAGE));
		*result_index = ret;
		return 0;
	} 
	return -ENOENT;
}

HCFS_STAT *generate_mock_stat(ino_t inode_num)
{
	HCFS_STAT *test_stat = (HCFS_STAT *)malloc(sizeof(HCFS_STAT));
	memset(test_stat, 0, sizeof(HCFS_STAT));
	test_stat->ino = inode_num;
	test_stat->nlink = inode_num + 3;
	test_stat->uid = inode_num + 6;
	test_stat->gid = inode_num + 9;
	test_stat->size = inode_num * 97;
	test_stat->mode = S_IFREG;
	
	return test_stat;
}

int32_t super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	if (inode_ptr == NULL)
		inode_ptr = (SUPER_BLOCK_ENTRY *)malloc(sizeof(SUPER_BLOCK_ENTRY));

	memcpy(&(inode_ptr->inode_stat), generate_mock_stat(this_inode), sizeof(HCFS_STAT));
	return 0;
}

int32_t write_log(int32_t level, const char *format, ...)
{
	return 0;
}

char block_finish_uploading(int fd, long long blockno)
{
	return MOCK_FINISH_UPLOADING;
}

int fetch_toupload_block_path(char *pathname, ino_t inode,
		long long block_no, long long seq)
{
	return 0;
}

void fetch_backend_block_objname(char *objname, ino_t inode,
		long long block_no, long long seqnum)
{
}

int check_and_copy_file(const char *srcpath, const char *tarpath,
		BOOL lock_src)
{
	return MOCK_RETURN_VAL;
}

int fetch_block_path(char *pathname, ino_t this_inode, int64_t block_num)
{
	return 0;
}
int32_t restore_meta_super_block_entry(ino_t this_inode,
                struct stat *ret_stat)
{
	return 0;
}
int32_t rebuild_parent_stat(ino_t this_inode, ino_t p_inode, int8_t d_type)
{
	num_stat_rebuilt++;
	return 0;
}
