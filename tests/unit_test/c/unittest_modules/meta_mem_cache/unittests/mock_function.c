#include <sys/stat.h>
#include <semaphore.h>
#include <stdio.h>
#include <errno.h>
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
	int i;
	for (i=0 ; i<num_entries ; i++) {
		if (strcmp(new_entry->d_name, entry_array[i].d_name) == 0) {
			memcpy(new_entry, &entry_array[i], sizeof(DIR_ENTRY));
			return i;
		}
	}
	return -1;
}

int search_dir_entry_btree(char *target_name, DIR_ENTRY_PAGE *tnode, int fh, int *result_index, DIR_ENTRY_PAGE *result_node)
{
	DIR_ENTRY tmp_entry;
	int tmp_index;
	int ret;

	strcpy(tmp_entry.d_name, target_name);
	ret = dentry_binary_search(tnode->dir_entries, tnode->num_entries,
		&tmp_entry, &tmp_index);
	if (ret >= 0) {
		memcpy(result_node, tnode, sizeof(DIR_ENTRY_PAGE));
		*result_index = ret;
		return 0;
	} 
	return -ENOENT;
}

struct stat *generate_mock_stat(ino_t inode_num)
{
	struct stat *test_stat = (struct stat *)malloc(sizeof(struct stat));
	memset(test_stat, 0, sizeof(struct stat));
	test_stat->st_ino = inode_num;
	test_stat->st_nlink = inode_num + 3;
	test_stat->st_uid = inode_num + 6;
	test_stat->st_gid = inode_num + 9;
	test_stat->st_size = inode_num * 97;
	test_stat->st_mode = S_IFREG;
	
	return test_stat;
}

int super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	if (inode_ptr == NULL)
		inode_ptr = (SUPER_BLOCK_ENTRY *)malloc(sizeof(SUPER_BLOCK_ENTRY));

	memcpy(&(inode_ptr->inode_stat), generate_mock_stat(this_inode), sizeof(struct stat));
	return 0;
}

int write_log(int level, char *format, ...)
{
	return 0;
}

char did_block_finish_uploading(int fd, long long blockno)
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
	return 0;
}

int check_and_copy_file(const char *srcpath, const char *tarpath,
		BOOL lock_src)
{
	return MOCK_RETURN_VAL;
}

int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	return 0;
}
