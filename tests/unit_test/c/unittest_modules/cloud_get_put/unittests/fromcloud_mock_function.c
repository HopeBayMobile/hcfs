#include <sys/stat.h>
#include <stdio.h>
#include <curl/curl.h>
#include <semaphore.h>
#include "hcfscurl.h"
#include "mock_params.h"
#include "enc.h"
#include "meta_mem_cache.h"

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	if (OPEN_META_PATH_FAIL == TRUE)
		strcpy(pathname, "");
	else
		strcpy(pathname, "/tmp/testHCFS/tmp_meta");
	return 0;
}

int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	if (OPEN_BLOCK_PATH_FAIL == TRUE)
		strcpy(pathname, "");
	else
		strcpy(pathname, "/tmp/testHCFS/tmp_block");
	return 0;
}

int hcfs_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle, HCFS_encode_object_meta *object_meta)
{
	int inode, block_no;

	if (FETCH_BACKEND_BLOCK_TESTING == TRUE)
		return 0;

	sscanf(objname, "data_%d_%d", &inode, &block_no);
	if (block_no == BLOCK_NUM__FETCH_SUCCESS) {
		ftruncate(fileno(fptr), EXTEND_FILE_SIZE);
		return HTTP_OK;
	} else if (block_no == BLOCK_NUM__FETCH_SUCCESS) {
		return HTTP_FAIL;
	} else {
		sem_wait(&objname_counter_sem);
		strcpy(objname_list[objname_counter], objname);
		objname_counter++;
		sem_post(&objname_counter_sem);
		return HTTP_OK;
	}
}

int sync_hcfs_system_data(char need_lock)
{
	return 0;
}
int write_log(int level, char *format, ...)
{
	return 0;
}

int decode_to_fd(FILE *fptr, unsigned char *key, unsigned char *input, int input_length, int enc_flag, int compress_flag){

  ftruncate(fileno(fptr), EXTEND_FILE_SIZE);
	return 0;
}

unsigned char *get_key(){
  return NULL;
}

void free_object_meta(HCFS_encode_object_meta *object_meta)
{
    return;
}

int decrypt_session_key(unsigned char *session_key, char *enc_session_key,
                    unsigned char *key){
    return 0;
}

int set_block_dirty_status(char *path, FILE *fptr, char status)
{
	fsetxattr(fileno(fptr), "user.dirty", "F", 1, 0);
	return 0;
}

META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	return (META_CACHE_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
}

int meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	if (target_ptr)
		free(target_ptr);
	return 0;
}

int update_file_stats(FILE *metafptr, long long num_blocks_delta,
		long long num_cached_blocks_delta,
		long long cached_size_delta)
{
	return 0;
}

void get_system_size(long long *cache_size, long long *pinned_size)
{
	if (CACHE_FULL == TRUE) {
		if (cache_size)
			*cache_size = CACHE_HARD_LIMIT;
	} else {
		if (cache_size)
			*cache_size = CACHE_HARD_LIMIT - 1;
	}

	return 0;
}

int fetch_error_download_path(char *path, ino_t inode)
{
	sprintf(path, "/tmp/mock_error_path_%ju", (uintmax_t)inode);
	return 0;
}

int super_block_mark_dirty(ino_t this_inode)
{
	return 0;
}

int meta_cache_lookup_file_data(ino_t this_inode, struct stat *inode_stat,
		FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
		long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (block_page) {
		block_page->block_entries[0].status = NOW_STATUS;
		if (NOW_STATUS == ST_CLOUD)
			NOW_STATUS = ST_CtoL;
		else if (NOW_STATUS == ST_CtoL)
			NOW_STATUS = ST_LDISK;
	}
	return 0;
}

int meta_cache_update_file_data(ino_t this_inode, const struct stat *inode_stat,
	const FILE_META_TYPE *file_meta_ptr, const BLOCK_ENTRY_PAGE *block_page,
	const long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int change_system_meta(long long system_size_delta,
		long long cache_size_delta, long long cache_blocks_delta)
{
	return 0;
}

long long seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr,
		long long target_page, long long hint_page)
{
	return sizeof(struct stat) + sizeof(FILE_META_TYPE);
}
