#include "atomic_tocloud.h"
#include "mock_params.h"
#include <inttypes.h>

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
	if (delete_which_one == DEL_BACKEND_BLOCKS)
		test_delete_struct.record_uploading_inode[test_delete_struct.total_inode++] = inode;
	sem_post(&test_delete_struct.record_sem);
	return 0;
}

long long seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr,
		long long target_page, long long hint_page)
{
	return (sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) +
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
	HCFS_STAT tmpstat;

	if (fetch_from_cloud_fail == TRUE)
		return -EIO;
	if (is_first_upload == TRUE)
		return -ENOENT;

	tmpstat.size = 10485760;
	pwrite(fileno(fptr), &tmpstat, sizeof(HCFS_STAT), 0);
	pwrite(fileno(fptr), &filemeta, sizeof(FILE_META_TYPE),
			sizeof(HCFS_STAT));

	return 0;
}

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	strcpy(pathname, "/tmp/mock_local_meta");
}

int fuseproc_set_uploading_info(const UPLOADING_COMMUNICATION_DATA *data)
{
	return 0;
}

int32_t change_system_meta(int64_t system_data_size_delta,
		int64_t meta_size_delta, int64_t cache_data_size_delta,
		int64_t cache_blocks_delta, int64_t dirty_cache_delta,
		int64_t unpin_dirty_data_size, BOOL need_sync)
{
	hcfs_system->systemdata.system_size += system_data_size_delta;
	hcfs_system->systemdata.cache_size += cache_data_size_delta;
	return 0;
}

int32_t unlink_upload_file(char *filename)
{
	unlink(filename);
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
