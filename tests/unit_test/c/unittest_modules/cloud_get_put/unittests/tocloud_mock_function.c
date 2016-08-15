#include "atomic_tocloud.h"
#include "enc.h"
#include "fuseop.h"
#include "global.h"
#include "hcfs_clouddelete.h"
#include "hcfs_tocloud.h"
#include "mock_params.h"
#include "mount_manager.h"
#include "params.h"
#include "super_block.h"
#include "ut_global.h"
#include <attr/xattr.h>
#include <inttypes.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "ut_global.h"

#define MOCK() DEBUG_PRINT("[MOCK] tocloud_mock_function.c line %4d func %s\n",  __LINE__, __func__)
int32_t fetch_meta_path(char *pathname, ino_t this_inode)
{
	MOCK();
	strcpy(pathname, MOCK_META_PATH);
	return 0;
}

int32_t fetch_block_path(char *pathname, ino_t this_inode, int64_t block_num)
{
	MOCK();
	char mock_block_path[50];
	FILE *ptr;
	sprintf(mock_block_path, "/tmp/testHCFS/data_%" PRIu64 "_%" PRId64,
		(uint64_t)this_inode, block_num);
	ptr = fopen(mock_block_path, "w+");
	truncate(mock_block_path, EXTEND_FILE_SIZE);
	fclose(ptr);
	strcpy(pathname, mock_block_path);
	return 0;
}

void con_object_dsync(DELETE_THREAD_TYPE *delete_thread_ptr)
{
	MOCK();
	// Record to-delete block number
	upload_ctl_todelete_blockno[delete_thread_ptr->blockno] = TRUE;
	delete_ctl.threads_in_use[delete_thread_ptr->which_curl] = FALSE;
	sem_post(&delete_ctl.delete_queue_sem);
	return;
}

int32_t hcfs_init_backend(CURL_HANDLE *curl_handle)
{
	MOCK();
	return HTTP_OK;
}

int32_t super_block_update_transit(ino_t this_inode, char is_start_transit,
	char transit_incomplete)
{
	MOCK();
	if (this_inode > 1 && transit_incomplete == FALSE) { // inode > 1 is used to test upload_loop()
		sem_wait(&shm_verified_data->record_inode_sem);
		shm_verified_data->record_handle_inode[shm_verified_data->record_inode_counter] = 
			this_inode; // Record the inode number to verify.
		shm_verified_data->record_inode_counter++;
		sem_post(&shm_verified_data->record_inode_sem);

		sys_super_block->head.num_dirty--;
		printf("Test: inode %zu is updated\n", this_inode);
	}
	return 0;
}

int32_t hcfs_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle, HTTP_meta *meta)
{
	MOCK();
	char objectpath[40];
	FILE *objptr;
	int32_t readsize1, readsize2;
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
int32_t do_block_delete(ino_t this_inode, int64_t block_no, uint8_t *obj_id,
		    CURL_HANDLE *curl_handle)
#else
int32_t do_block_delete(ino_t this_inode, int64_t block_no, int64_t seq,
		    CURL_HANDLE *curl_handle)
#endif
{
	MOCK();
	char deleteobjname[30];
	sprintf(deleteobjname, "data_%" PRIu64 "_%ld", (uint64_t)this_inode, block_no);
	printf("Test: mock data %s is deleted\n", deleteobjname);

	usleep(200000); // Let thread busy
	sem_wait(&objname_counter_sem);
	strcpy(objname_list[objname_counter], deleteobjname);
	objname_counter++;
	sem_post(&objname_counter_sem);
	return 0;
}

int32_t super_block_exclusive_locking(void)
{
	MOCK();
	return 0;
}

int32_t read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	MOCK();
	if (this_inode == 0)
		return -1;

	inode_ptr->status = IS_DIRTY;
	inode_ptr->in_transit = FALSE;
	(inode_ptr->inode_stat).mode = S_IFDIR;

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

int32_t write_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	MOCK();
	return 0;
}

int32_t super_block_exclusive_release(void)
{
	MOCK();
	return 0;
}

/* A mock function to return linear block indexing */
int64_t seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr, 
	int64_t target_page, int64_t hint_page)
{
	MOCK();
	int64_t ret_page_pos;

	/* target_page starts from 0 */
	if (target_page >= mock_total_page)
		return 0;

	ret_page_pos = sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) +
		sizeof(FILE_STATS_TYPE) + sizeof(CLOUD_RELATED_DATA) +
		target_page * sizeof(BLOCK_ENTRY_PAGE);

	return ret_page_pos;
}

int32_t write_log(int32_t level, char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

int32_t hcfs_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle, HCFS_encode_object_meta *object_meta)
{
	MOCK();
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

int32_t set_block_dirty_status(char *path, FILE *fptr, char status)
{
	MOCK();
	setxattr(path, "user.dirty", "F", 1, 0);
	return 0;
}

int32_t fetch_trunc_path(char *pathname, ino_t this_inode)
{
	MOCK();
	strcpy(pathname, "/tmp/testHCFS/mock_trunc");
	return 0;
}

off_t check_file_size(const char *path)
{
	MOCK();
	struct stat block_stat; /* raw file ops */
	int32_t errcode;

	errcode = stat(path, &block_stat);
	if (errcode == 0)
		return block_stat.st_size;
	errcode = errno;
	write_log(0, "Error when checking file size. Code %d, %s\n",
			errcode, strerror(errcode));
	return -errcode;
}
int32_t sync_hcfs_system_data(char need_lock)
{
	MOCK();
	return 0;
}

int32_t backup_FS_database(void)
{
	MOCK();
	return 0;
}

void fetch_backend_block_objname(char *objname, ino_t inode,
		long long block_no, long long seqnum)
{
	MOCK();
	sprintf(objname, "data_%"PRIu64"_%lld", (uint64_t)inode, block_no);
	return;
}

int32_t fetch_toupload_block_path(char *pathname, ino_t inode,
		int64_t block_no, int64_t seq)
{
	MOCK();
	sprintf(pathname,
		"mock_meta_folder/mock_toupload_block_%" PRIu64 "_%" PRId64,
		inode, block_no);
	return 0;
}

int fetch_toupload_meta_path(char *pathname, ino_t inode)
{
	MOCK();
	sprintf(pathname, "mock_meta_folder/mock_toupload_meta_%"PRIu64,
			(uint64_t)inode);

	return 0;
}

int comm2fuseproc(ino_t this_inode, BOOL is_uploading,
		int fd, BOOL is_revert, BOOL finish_sync)
{
	MOCK();
	return 0;
}

int del_progress_file(int fd, ino_t inode)
{
	MOCK();
	close(fd);
	unlink("/tmp/mock_progress_file");
	return 0;
}

int32_t set_progress_info(int32_t fd, int64_t block_index,
	const char *toupload_exist, const char *backend_exist,
	const int64_t *toupload_seq, const int64_t *backend_seq,
	const char *finish)
{
	MOCK();
	BLOCK_UPLOADING_STATUS block_entry;

	if (toupload_seq)
		block_entry.to_upload_seq = *toupload_seq;
	if (backend_seq)
		block_entry.backend_seq = *backend_seq;
	if (finish)
		block_entry.finish_uploading = *finish;

	/* Linear mock */	
	pwrite(fd, &block_entry, sizeof(BLOCK_UPLOADING_STATUS),
			block_index * sizeof(BLOCK_UPLOADING_STATUS));

	return 0;
}

int64_t query_status_page(int32_t fd, int64_t block_index)
{
	MOCK();
	return (block_index / MAX_BLOCK_ENTRIES_PER_PAGE) *
		sizeof(BLOCK_UPLOADING_PAGE);
}

int32_t init_backend_file_info(const SYNC_THREAD_TYPE *ptr,
		int64_t *backend_size, int64_t *total_backend_blocks,
		int64_t upload_seq, uint8_t *last_pin_status)
{
	MOCK();
	return 0;
}

int check_and_copy_file(const char *srcpath, const char *tarpath,
		BOOL lock_src, BOOL reject_if_nospc)
{
	MOCK();
	mknod(tarpath, 0700, 0);
	return 0;
}

void fetch_progress_file_path(char *pathname, ino_t inode)
{
	MOCK();
	pathname[0] = 0;
	return;
}

char block_finish_uploading(int32_t fd, int64_t blockno)
{
	MOCK();
	return TRUE;
}

int create_progress_file(ino_t inode)
{
	MOCK();
	return 0;
}

void continue_inode_sync(SYNC_THREAD_TYPE *data_ptr)
{
	MOCK();
	return;
}

int change_action(int fd, char now_action)
{
	MOCK();
	return 0;
}

int change_status_to_BOTH(ino_t inode, int progress_fd,
		FILE *local_metafptr, char *local_metapath)
{
	MOCK();
	FILE_META_TYPE filemeta;
	BLOCK_ENTRY_PAGE block_page;
	int i;
	long long pos;
	long long page_count;
	size_t ret_size;

	printf("Begin to change status to BOTH\n");
	page_count = 0;
	fseek(local_metafptr, sizeof(HCFS_STAT), SEEK_SET);
	fread(&filemeta, sizeof(FILE_META_TYPE), 1, local_metafptr);
	while (!feof(local_metafptr)) {
		/* Linearly read block meta */
		fseek(local_metafptr, sizeof(HCFS_STAT) +
			sizeof(FILE_META_TYPE) + sizeof(FILE_STATS_TYPE) +
			sizeof(CLOUD_RELATED_DATA) + page_count *
			sizeof(BLOCK_ENTRY_PAGE), SEEK_SET);
		ret_size = fread(&block_page, 1, sizeof(BLOCK_ENTRY_PAGE),
				local_metafptr);
		if (ret_size != sizeof(BLOCK_ENTRY_PAGE))
			break;

		for (i = 0 ; i < block_page.num_entries ; i++) {
			if (block_page.block_entries[i].status == ST_LtoC)
				block_page.block_entries[i].status = ST_BOTH;
		}
		fseek(local_metafptr, sizeof(HCFS_STAT) +
			sizeof(FILE_META_TYPE) + sizeof(FILE_STATS_TYPE) +
			sizeof(CLOUD_RELATED_DATA) + page_count *
			sizeof(BLOCK_ENTRY_PAGE), SEEK_SET);
		fwrite(&block_page, 1, sizeof(BLOCK_ENTRY_PAGE), local_metafptr);

		page_count++;
	}

	return 0;
}

int32_t change_block_status_to_BOTH(ino_t inode, int64_t blockno,
		int64_t page_pos, int64_t toupload_seq)
{
	FILE *fptr;
	BLOCK_ENTRY_PAGE tmppage;
	int i;

	MOCK();
	i = blockno % MAX_BLOCK_ENTRIES_PER_PAGE;

	if (page_pos <= 0)
		return 0;

	fptr = fopen(MOCK_META_PATH, "r+");
	if (!fptr)
		return 0;
	setbuf(fptr, NULL);
	flock(fileno(fptr), LOCK_EX);
	fseek(fptr, page_pos, SEEK_SET);
	fread(&tmppage, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	if (tmppage.block_entries[i].status == ST_LtoC)
		tmppage.block_entries[i].status = ST_BOTH;
	fseek(fptr, page_pos, SEEK_SET);
	fwrite(&tmppage, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);

	return 0;
}

int delete_backend_blocks(int progress_fd, long long total_blocks, ino_t inode,
		char delete_which_one)
{
	MOCK();
	return 0;
}

void busy_wait_all_specified_upload_threads(ino_t inode)
{
	MOCK();
	return;
}

int revert_block_status_LDISK(ino_t this_inode, long long blockno,
		int e_index, long long page_filepos)
{
	MOCK();
	return 0;
}

int32_t update_backend_usage(int64_t total_backend_size_delta,
		int64_t meta_size_delta, int64_t num_inodes_delta)
{
	MOCK();
	return 0;
}

int32_t update_fs_backend_usage(FILE *fptr, int64_t fs_total_size_delta,
		int64_t fs_meta_size_delta, int64_t fs_num_inodes_delta,
		int64_t fs_pin_size_delta)
{
	FS_CLOUD_STAT_T	fs_cloud_stat;

	MOCK();
	fseek(fptr, 0, SEEK_SET);
	fread(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
	fs_cloud_stat.backend_system_size += fs_total_size_delta;
	fs_cloud_stat.backend_meta_size += fs_meta_size_delta;
	fs_cloud_stat.backend_num_inodes += fs_num_inodes_delta;
	fs_cloud_stat.pinned_size += fs_pin_size_delta;
	fseek(fptr, 0, SEEK_SET);
	fwrite(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);

	return 0;
}

int32_t update_file_stats(FILE *metafptr, int64_t num_blocks_delta,
			int64_t num_cached_blocks_delta,
			int64_t cached_size_delta,
			int64_t dirty_data_size_delta,
			ino_t thisinode)
{
	MOCK();
	return 0;
}

int32_t get_progress_info(int32_t fd, int64_t block_index,
		BLOCK_UPLOADING_STATUS *block_uploading_status)
{
	MOCK();
	return 0;
}

int32_t change_system_meta(int64_t system_data_size_delta,
		int64_t meta_size_delta, int64_t cache_data_size_delta,
		int64_t cache_blocks_delta, int64_t dirty_cache_delta,
		int64_t unpin_dirty_data_size, BOOL need_sync)
{
	MOCK();
	return 0;
}

void notify_sleep_on_cache(int32_t cache_replace_status)
{
	MOCK();
	return;
}

ino_t pull_retry_inode(IMMEDIATELY_RETRY_LIST *list)
{
	return 0;
}

void push_retry_inode(IMMEDIATELY_RETRY_LIST *list, ino_t inode)
{
	return;
}

int32_t super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	return 0;
}
