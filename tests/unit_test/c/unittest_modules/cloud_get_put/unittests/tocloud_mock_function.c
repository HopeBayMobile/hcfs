#include <sys/stat.h>
#include "hcfs_tocloud.h"
#include "hcfs_clouddelete.h"
#include "params.h"
#include "mock_params.h"
#include "super_block.h"
#include "global.h"


int fetch_meta_path(char *pathname, ino_t this_inode)
{
	strcpy(pathname, "/tmp/mock_file_meta");
	return 0;
}

int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	char mock_block_path[50];
	FILE *ptr;
	sprintf(mock_block_path, "/tmp/data_%d_%d", this_inode, block_num);
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
	return;
}

int hcfs_init_backend(CURL_HANDLE *curl_handle)
{
	return HTTP_OK;
}

int super_block_update_transit(ino_t this_inode, char is_start_transit)
{
	if (this_inode > 1) { // inode > 1 is used to test upload_loop()
		sem_wait(&shm_verified_data->record_inode_sem);
		shm_verified_data->record_handle_inode[shm_verified_data->record_inode_counter] = 
			this_inode;
		shm_verified_data->record_inode_counter++;
		sem_post(&shm_verified_data->record_inode_sem);
		printf("Test: inode %d is deleted\n", this_inode);
	}
	return 0;
}

int hcfs_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
{
	sem_wait(&objname_counter_sem);
	strcpy(objname_list[objname_counter], objname);
	objname_counter++;
	sem_post(&objname_counter_sem);
	//usleep(1000000);
	return 0;
}

void do_block_delete(ino_t this_inode, long long block_no, CURL_HANDLE *curl_handle)
{
	char deleteobjname[30];
	sprintf(deleteobjname, "data_%d_%d", this_inode, block_no);
	printf("Test: mock data %s is deleted\n", deleteobjname);

	usleep(200000); // Let thread busy
	sem_wait(&objname_counter_sem);
	strcpy(objname_list[objname_counter], deleteobjname);
	objname_counter++;
	sem_post(&objname_counter_sem);
	return ;
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
	
	if (shm_test_data->tohandle_counter == shm_test_data->num_inode) {
		inode_ptr->util_ll_next = 0;
		sys_super_block->head.first_dirty_inode = 0;
	} else {
		inode_ptr->util_ll_next = shm_test_data->to_handle_inode[shm_test_data->tohandle_counter];
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
