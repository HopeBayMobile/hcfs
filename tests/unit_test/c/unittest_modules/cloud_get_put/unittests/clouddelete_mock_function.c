#include <sys/stat.h>
#include <stdio.h>
#include <curl/curl.h>
#include <semaphore.h>
#include <inttypes.h>
#include "hcfscurl.h"
#include "mock_params.h"
#include "super_block.h"
#include "params.h"
#include "fuseop.h"
#include "tocloud_tools.h"

int32_t hcfs_init_backend(CURL_HANDLE *curl_handle)
{
	return HTTP_OK;
}


void hcfs_destroy_backend(CURL_HANDLE *curl_handle)
{
	return;
}

int32_t fetch_todelete_path(char *pathname, ino_t this_inode)
{
	if (this_inode == INODE__FETCH_TODELETE_PATH_SUCCESS) {
		strcpy(pathname, TODELETE_PATH);
		return 0;
	} else if (this_inode == INODE__FETCH_TODELETE_PATH_FAIL) {
		pathname[0] = '\0';
		return -1;
	} else {
		/* Record inode. Called when deleting inode in delete_loop() */
		usleep(500000); // Let threads busy
		sem_wait(&(to_verified_data.record_inode_sem));
		to_verified_data.record_handle_inode[to_verified_data.record_inode_counter] = this_inode;
		to_verified_data.record_inode_counter++;
		sem_post(&(to_verified_data.record_inode_sem));
		pathname[0] = '\0';
		printf("Test: mock inode %d is deleted\n", this_inode);
		return -1;
	}
}

int32_t super_block_delete(ino_t this_inode)
{
	return 0;
}

int32_t super_block_reclaim(void)
{
	return 0;
}

int32_t hcfs_delete_object(char *objname, CURL_HANDLE *curl_handle)
{
	sem_wait(&objname_counter_sem);
	strcpy(objname_list[objname_counter], objname);
	objname_counter++;
	sem_post(&objname_counter_sem);

	return 200;
}

int32_t super_block_share_locking(void)
{
	return 0;
}

int32_t read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	if (this_inode == 0)
		return -1;
	if (test_data.tohandle_counter == test_data.num_inode) {
		inode_ptr->status = TO_BE_DELETED;
		inode_ptr->util_ll_next = 0;
		sys_super_block->head.first_to_delete_inode = 0;
	} else {
		inode_ptr->status = TO_BE_DELETED;
		inode_ptr->util_ll_next = test_data.to_handle_inode[test_data.tohandle_counter];
		test_data.tohandle_counter++;
	}
	return 0;
}

int32_t super_block_share_release(void)
{
	return 0;
}

/* A mock function to return linear block indexing */
int64_t seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr, 
	int64_t target_page, int64_t hint_page) 
{
	if (target_page >= 3)
		return 0;
	long long ret_page_pos = sizeof(struct stat) + 
		sizeof(FILE_META_TYPE) + sizeof(CLOUD_RELATED_DATA) +
		target_page * sizeof(BLOCK_ENTRY_PAGE);
	return ret_page_pos;
}

int32_t write_log(int32_t level, char *format, ...)
{
	return 0;
}

int32_t update_backend_stat(ino_t root_inode, int64_t system_size_delta,
			int64_t num_inodes_delta)
{
	return 0;
}

int32_t fetch_trunc_path(char *pathname, ino_t this_inode)
{
	strcpy(pathname, "/tmp/testHCFS/mock_trunc");
	return 0;
}

void nonblock_sleep(uint32_t secs, BOOL (*wakeup_condition)())
{
	sleep(secs);
	return;
}

int fetch_toupload_meta_path(char *pathname, ino_t inode)
{
	return 0;
}

void fetch_backend_meta_objname(char *objname, ino_t inode)
{
	return;
}

void fetch_backend_block_objname(char *objname,
	ino_t inode, long long block_no, long long seqnum)
{
	sprintf(objname, "data_%"PRIu64"_%lld", (uint64_t)inode, block_no);
	return;
}

int fetch_backend_meta_path(char *pathname, ino_t inode)
{
	return 0;
}

void fetch_progress_file_path(char *pathname, ino_t inode)
{
	return 0;
}

int fetch_from_cloud(FILE *fptr, char action_from, char *objname)
{
	FILE *src;
	char buf[4100];
	size_t size;

	src = fopen(TODELETE_PATH, "r");

	fseek(fptr, 0, SEEK_SET);
	fseek(src, 0, SEEK_SET);
	while (size = fread(buf, 1, 4096, src)) {
		fwrite(buf, 1, size, fptr);
	}

	close(src);

	return 0;
}

void fetch_del_backend_meta_path(char *backend_metapath, ino_t this_inode)
{
	sprintf(backend_metapath, "/tmp/mock_backend_meta");
}

ino_t pull_retry_inode(IMMEDIATELY_RETRY_LIST *list)
{
	return 0;
}

void push_retry_inode(IMMEDIATELY_RETRY_LIST *list, ino_t inode)
{
	return;
}
