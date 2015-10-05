#include "hcfs_tocloud.h"
#include "mock_params.h"

int write_log(int level, char *format, ...)
{
	return 0;
}

void sync_single_inode(SYNC_THREAD_TYPE *ptr)
{
	char toupload_metapath[200];

	sem_wait(&test_sync_struct.record_sem);
	test_sync_struct.record_uploading_inode[test_sync_struct.total_inode++] = ptr->inode;
	sem_post(&test_sync_struct.record_sem);

	fetch_toupload_meta_path(toupload_metapath, ptr->inode);
	unlink(toupload_metapath);

	return;
}

int delete_backend_blocks(int progress_fd, long long total_blocks, ino_t inode,
		char delete_which_one)
{
	sem_wait(&test_delete_struct.record_sem);
	if (delete_which_one == BACKEND_BLOCKS)
	test_delete_struct.record_uploading_inode[test_delete_struct.total_inode++] = inode;
	sem_post(&test_delete_struct.record_sem);
	return 0;
}

long long seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr,
		long long target_page, long long hint_page)
{
	return (sizeof(struct stat) + sizeof(FILE_META_TYPE) +
		target_page * sizeof(BLOCK_ENTRY_PAGE));
}

int check_page_level(long long page_index)
{
	long long tmp_index;

	if (page_index == 0)
		return 0;   

	tmp_index = page_index - 1; 

	if (tmp_index < POINTERS_PER_PAGE)
		return 1;

	tmp_index = tmp_index - POINTERS_PER_PAGE;

	if (tmp_index < (POINTERS_PER_PAGE * POINTERS_PER_PAGE)) 
		return 2;

	tmp_index = tmp_index - (POINTERS_PER_PAGE * POINTERS_PER_PAGE); 

	if (tmp_index < (POINTERS_PER_PAGE * POINTERS_PER_PAGE *
		POINTERS_PER_PAGE))
		return 3;

	tmp_index = tmp_index - (POINTERS_PER_PAGE * POINTERS_PER_PAGE *
		POINTERS_PER_PAGE); 

	return 4;
}
