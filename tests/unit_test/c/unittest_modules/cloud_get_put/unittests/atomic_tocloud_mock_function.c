#include "atomic_tocloud.h"
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

	return;
}

int delete_backend_blocks(int progress_fd, long long total_blocks, ino_t inode,
		char delete_which_one)
{
	sem_wait(&test_delete_struct.record_sem);
	if (delete_which_one == DEL_BACKEND_BLOCKS)
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

void fetch_backend_meta_objname(char *objname, ino_t inode)
{
	sprintf(objname, "/tmp/mock_backend_meta_%"PRIu64, (uint64_t)inode);
	return;
}

int fetch_from_cloud(FILE *fptr, char action_from, char *objname)
{
	FILE_META_TYPE filemeta;
	struct stat tmpstat;

	if (fetch_from_cloud_fail == TRUE)
		return -EIO;
	if (is_first_upload == TRUE)
		return -ENOENT;

	tmpstat.st_size = 10485760;
	pwrite(fileno(fptr), &tmpstat, sizeof(struct stat), 0);
	pwrite(fileno(fptr), &filemeta, sizeof(FILE_META_TYPE),
			sizeof(struct stat));

	return 0;
}

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	strcpy(pathname, "/tmp/mock_local_meta");
}
