#include <sys/stat.h>
#include <stdio.h>
#include <curl/curl.h>
#include <semaphore.h>
#include "hcfscurl.h"
#include "mock_params.h"
#include "super_block.h"
#include "params.h"
#include "fuseop.h"

int hcfs_init_backend(CURL_HANDLE *curl_handle)
{
	return HTTP_OK;
}


void hcfs_destroy_backend(CURL_HANDLE *curl_handle)
{
	return;
}

int fetch_todelete_path(char *pathname, ino_t this_inode)
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

int super_block_delete(ino_t this_inode)
{
	return 0;
}

int super_block_reclaim(void)
{
	return 0;
}

int hcfs_delete_object(char *objname, CURL_HANDLE *curl_handle)
{
	sem_wait(&objname_counter_sem);
	strcpy(objname_list[objname_counter], objname);
	objname_counter++;
	sem_post(&objname_counter_sem);

	return 0;
}

int super_block_share_locking(void)
{
	return 0;
}

int read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	if (this_inode == 0)
		return -1;
	if (test_data.tohandle_counter == test_data.num_inode) {
		inode_ptr->status = TO_BE_DELETED;
		inode_ptr->util_ll_next = 0;
	} else {
		inode_ptr->status = TO_BE_DELETED;
		inode_ptr->util_ll_next = test_data.to_handle_inode[test_data.tohandle_counter];
		test_data.tohandle_counter++;
	}
	return 0;
}

int super_block_share_release(void)
{
	return 0;
}

/* A mock function to return linear block indexing */
long long seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr, 
	long long target_page, long long hint_page) 
{
	if (target_page >= 3)
		return 0;
	long long ret_page_pos = sizeof(struct stat) + 
		sizeof(FILE_META_TYPE) + target_page *
		sizeof(BLOCK_ENTRY_PAGE);
	return ret_page_pos;
}

int write_log(int level, char *format, ...)
{
	return 0;
}

int update_backend_stat(ino_t root_inode, long long system_size_delta,
			long long num_inodes_delta)
{
	return 0;
}

int fetch_trunc_path(char *pathname, ino_t this_inode)
{
	strcpy(pathname, "/tmp/testHCFS/mock_trunc");
	return 0;
}

