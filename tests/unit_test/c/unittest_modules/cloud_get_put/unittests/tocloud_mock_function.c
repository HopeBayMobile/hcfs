#include <sys/stat.h>
#include <stdarg.h>
#include <inttypes.h>
#include "hcfs_tocloud.h"
#include "hcfs_clouddelete.h"
#include "params.h"
#include "mock_params.h"
#include "super_block.h"
#include "global.h"
#include "fuseop.h"
#include "enc.h"
#include "mount_manager.h"

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	strcpy(pathname, "/tmp/testHCFS/mock_file_meta");
	return 0;
}

int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	char mock_block_path[50];
	FILE *ptr;
	sprintf(mock_block_path, "/tmp/testHCFS/data_%" PRIu64 "_%lld",
		(uint64_t)this_inode, block_num);
	ptr = fopen(mock_block_path, "w+");
	truncate(mock_block_path, EXTEND_FILE_SIZE);
	fclose(ptr);
	strcpy(pathname, mock_block_path);
	return 0;
}

void con_object_dsync(DELETE_THREAD_TYPE *delete_thread_ptr)
{
	// Record to-delete block number
	upload_ctl_todelete_blockno[delete_thread_ptr->blockno] = TRUE;
	delete_ctl.threads_in_use[delete_thread_ptr->which_curl] = FALSE;
	sem_post(&delete_ctl.delete_queue_sem);
	return;
}

int hcfs_init_backend(CURL_HANDLE *curl_handle)
{
	return HTTP_OK;
}

int super_block_update_transit(ino_t this_inode, char is_start_transit,
	char transit_incomplete)
{
	if (this_inode > 1) { // inode > 1 is used to test upload_loop()
		sem_wait(&shm_verified_data->record_inode_sem);
		shm_verified_data->record_handle_inode[shm_verified_data->record_inode_counter] = 
			this_inode; // Record the inode number to verify.
		shm_verified_data->record_inode_counter++;
		sem_post(&shm_verified_data->record_inode_sem);

		sys_super_block->head.num_dirty--;
		printf("Test: inode %d is updated\n", this_inode);
	}
	return 0;
}

int hcfs_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle, HTTP_meta *meta)
{
	char objectpath[40];
	FILE *objptr;
	int readsize1, readsize2;
	char filebuf1[4096], filebuf2[4096];

	if (strncmp(objname, "FSstat", 6) == 0)
		return 200;
	sprintf(objectpath, "/tmp/testHCFS/%s", objname);
	if (access(objectpath, F_OK) < 0) {
		if (access(MOCK_META_PATH, F_OK) < 0)
			return 0;
	}

	sem_wait(&objname_counter_sem);
	strcpy(objname_list[objname_counter], objname);
	objname_counter++;
	sem_post(&objname_counter_sem);
	hcfs_system->systemdata.dirty_cache_size = 0;
	return 200;
}

#if (DEDUP_ENABLE)
int do_block_delete(ino_t this_inode, long long block_no, unsigned char *obj_id,
		    CURL_HANDLE *curl_handle)
#else
int do_block_delete(ino_t this_inode, long long block_no,
		    CURL_HANDLE *curl_handle)
#endif
{
	char deleteobjname[30];
	sprintf(deleteobjname, "data_%" PRIu64 "_%lld", (uint64_t)this_inode, block_no);
	printf("Test: mock data %s is deleted\n", deleteobjname);

	usleep(200000); // Let thread busy
	sem_wait(&objname_counter_sem);
	strcpy(objname_list[objname_counter], deleteobjname);
	objname_counter++;
	sem_post(&objname_counter_sem);
	return 0;
}

int super_block_exclusive_locking(void)
{
	return 0;
}

int read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	if (this_inode == 0)
		return -1;

	inode_ptr->status = IS_DIRTY;
	inode_ptr->in_transit = FALSE;
	(inode_ptr->inode_stat).st_mode = S_IFDIR;

	if (shm_test_data->tohandle_counter == shm_test_data->num_inode) {
		inode_ptr->util_ll_next = 0;
		sys_super_block->head.first_dirty_inode = 0;
	} else {
		inode_ptr->util_ll_next = 
			shm_test_data->to_handle_inode[shm_test_data->tohandle_counter];
		shm_test_data->tohandle_counter++;
	}
	return 0;
}

int write_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	return 0;
}

int super_block_exclusive_release(void)
{
	return 0;
}

/* A mock function to return linear block indexing */
long long seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr, 
	long long target_page, long long hint_page)
{
	long long ret_page_pos;

	/* target_page starts from 0 */
	if (target_page >= mock_total_page)
		return 0;

	ret_page_pos = sizeof(struct stat) + sizeof(FILE_META_TYPE) +
		sizeof(FILE_STATS_TYPE) + sizeof(FS_CLOUD_STAT_T) +
		target_page * sizeof(BLOCK_ENTRY_PAGE);

	return ret_page_pos;
}

int write_log(int level, char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

int hcfs_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle, HCFS_encode_object_meta *object_meta)
{
	FS_CLOUD_STAT_T fs_cloud_stat;

	if (no_backend_stat == TRUE)
		return 404;

	fs_cloud_stat.backend_system_size = 7687483;
	fs_cloud_stat.backend_meta_size = 5566;
	fs_cloud_stat.backend_num_inodes = 34334;
	fseek(fptr, 0, SEEK_SET);
	fwrite(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);

	return 200;
}

int set_block_dirty_status(char *path, FILE *fptr, char status)
{
	setxattr(path, "user.dirty", "F", 1, 0);
	return 0;
}

int fetch_trunc_path(char *pathname, ino_t this_inode)
{
	strcpy(pathname, "/tmp/testHCFS/mock_trunc");
	return 0;
}

off_t check_file_size(const char *path)
{
	struct stat block_stat;
	int errcode;

	errcode = stat(path, &block_stat);
	if (errcode == 0)
		return block_stat.st_size;
	errcode = errno;
	write_log(0, "Error when checking file size. Code %d, %s\n",
			errcode, strerror(errcode));
	return -errcode;
}
int sync_hcfs_system_data(char need_lock)
{
	return 0;
}
int backup_FS_database(void)
{
	return 0;
}

int update_backend_usage(long long total_backend_size_delta,
		long long meta_size_delta, long long num_inodes_delta)
{
	return 0;
}

int update_fs_backend_usage(FILE *fptr, long long fs_total_size_delta,
		long long fs_meta_size_delta, long long fs_num_inodes_delta)
{
	FS_CLOUD_STAT_T	fs_cloud_stat;

	fseek(fptr, 0, SEEK_SET);
	fread(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
	fs_cloud_stat.backend_system_size += fs_total_size_delta;
	fs_cloud_stat.backend_meta_size += fs_meta_size_delta;
	fs_cloud_stat.backend_num_inodes += fs_num_inodes_delta;
	fseek(fptr, 0, SEEK_SET);
	fwrite(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);

	return 0;
}

int update_file_stats(FILE *metafptr, long long num_blocks_delta,
			long long num_cached_blocks_delta,
			long long cached_size_delta,
			long long dirty_data_size_delta,
			ino_t thisinode)
{
	return 0;
}
