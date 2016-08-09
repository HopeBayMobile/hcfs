#include <sys/stat.h>
#include <stdio.h>
#include <curl/curl.h>
#include <semaphore.h>
#include <inttypes.h>
#include <jansson.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include "hcfscurl.h"
#include "mock_params.h"
#include "enc.h"
#include "meta_mem_cache.h"

#define MOCK() printf("[MOCK] fromcloud_mock_function.c:%4d %s\n",  __LINE__, __func__)
int32_t fetch_meta_path(char *pathname, ino_t this_inode)
{
	MOCK();
	if (OPEN_META_PATH_FAIL == TRUE)
		strcpy(pathname, "");
	else
		strcpy(pathname, "/tmp/testHCFS/tmp_meta");
	return 0;
}

int32_t fetch_block_path(char *pathname, ino_t this_inode, int64_t block_num)
{
	MOCK();
	if (OPEN_BLOCK_PATH_FAIL == TRUE)
		strcpy(pathname, "");
	else
		strcpy(pathname, "/tmp/testHCFS/tmp_block");
	return 0;
}

int32_t hcfs_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle, HCFS_encode_object_meta *object_meta)
{
	MOCK();
	int32_t inode, block_no;

	if (FETCH_BACKEND_BLOCK_TESTING == TRUE)
		return 200;

	if (!strncmp("user", objname, 4)) {
		if (usermeta_notfound)
			return 404;
		else
			return 200;
	}

	sscanf(objname, "data_%d_%d", &inode, &block_no);
	if (block_no == BLOCK_NUM__FETCH_SUCCESS) {
		ftruncate(fileno(fptr), EXTEND_FILE_SIZE);
		return HTTP_OK;
	} else {
		sem_wait(&objname_counter_sem);
		strcpy(objname_list[objname_counter], objname);
		objname_counter++;
		sem_post(&objname_counter_sem);
		return HTTP_OK;
	}
}

int32_t sync_hcfs_system_data(char need_lock)
{
	MOCK();
	return 0;
}
int32_t write_log(int32_t level, char *format, ...)
{
	MOCK();
	return 0;
}

int32_t decode_to_fd(FILE *fptr, uint8_t *key, uint8_t *input, int32_t input_length, int32_t enc_flag, int32_t compress_flag){

  ftruncate(fileno(fptr), EXTEND_FILE_SIZE);
	return 0;
}

uint8_t *get_key(const char *keywords){
  return NULL;
}

void free_object_meta(HCFS_encode_object_meta *object_meta)
{
	MOCK();
    return;
}

int32_t decrypt_session_key(uint8_t *session_key, char *enc_session_key,
                    uint8_t *key){
    return 0;
}

int32_t set_block_dirty_status(char *path, FILE *fptr, char status)
{
	MOCK();
	fsetxattr(fileno(fptr), "user.dirty", "F", 1, 0);
	return 0;
}

META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	MOCK();
	return (META_CACHE_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
}

int32_t meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	MOCK();
	if (target_ptr)
		free(target_ptr);
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

void get_system_size(int64_t *cache_size, int64_t *pinned_size)
{
	MOCK();
	if (CACHE_FULL == TRUE) {
		if (cache_size)
			*cache_size = CACHE_HARD_LIMIT;
	} else {
		if (cache_size)
			*cache_size = CACHE_HARD_LIMIT - 1;
	}
}

int32_t fetch_error_download_path(char *path, ino_t inode)
{
	MOCK();
	sprintf(path, "/tmp/mock_error_path_%ju", (uintmax_t)inode);
	return 0;
}

int32_t super_block_mark_dirty(ino_t this_inode)
{
	MOCK();
	return 0;
}

int32_t meta_cache_lookup_file_data(ino_t this_inode,
				    HCFS_STAT *inode_stat,
				    FILE_META_TYPE *file_meta_ptr,
				    BLOCK_ENTRY_PAGE *block_page,
				    int64_t page_pos,
				    META_CACHE_ENTRY_STRUCT *body_ptr)
{
	MOCK();
	if (block_page) {
		block_page->block_entries[0].status = NOW_STATUS;
		if (NOW_STATUS == ST_CLOUD)
			NOW_STATUS = ST_CtoL;
		else if (NOW_STATUS == ST_CtoL)
			NOW_STATUS = ST_BOTH;
	}
	return 0;
}

int32_t meta_cache_update_file_nosync(ino_t this_inode,
				      const HCFS_STAT *inode_stat,
				      const FILE_META_TYPE *file_meta_ptr,
				      const BLOCK_ENTRY_PAGE *block_page,
				      const int64_t page_pos,
				      META_CACHE_ENTRY_STRUCT *body_ptr)
{
	MOCK();
	return 0;
}

int32_t meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
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

int64_t seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr,
		int64_t target_page, int64_t hint_page)
{
	MOCK();
	return sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE);
}

void fetch_backend_block_objname(char *objname, ino_t inode,
		long long block_no, long long seqnum)
{
	MOCK();
	sprintf(objname, "data_%"PRIu64"_%lld", (uint64_t)inode, block_no);
	return;
}

void json_delete(json_t *json)
{
	MOCK();
}

json_t *json_loads(const char *input, size_t flags, json_error_t *error)
{
	MOCK();
	return (json_t*)1;
}

json_t *json_object_get(const json_t *object, const char *key)
{
	MOCK();
	json_t *ret;

	//if (json_file_corrupt)
	//	return NULL;

	ret = malloc(sizeof(json_t));
	ret->type = JSON_INTEGER; 
	return ret; 
}

json_int_t json_integer_value(const json_t *integer)
{
	MOCK();
	free((void *)integer);
	return 5566;
}

json_t *json_loadf(FILE *input, size_t flags, json_error_t *error)
{
	MOCK();
	return (json_t *)1;
}

char *json_dumps(const json_t *root, size_t flags)
{
	MOCK();
	return (char *)malloc(1);
}

void nonblock_sleep(uint32_t secs, BOOL (*wakeup_condition)(void))
{
	MOCK();
	sleep(1);
	return;
}

int32_t enc_backup_usermeta(char *json_str)
{
	MOCK();
	return 0;
}
