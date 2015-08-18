#include <sys/stat.h>
#include <stdarg.h>
#include "hcfs_tocloud.h"
#include "hcfs_clouddelete.h"
#include "params.h"
#include "mock_params.h"
#include "super_block.h"
#include "global.h"
#include "fuseop.h"

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	strcpy(pathname, "/tmp/testHCFS/mock_file_meta");
	return 0;
}

int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	char mock_block_path[50];
	FILE *ptr;
#ifdef ARM_32bit_
	sprintf(mock_block_path, "/tmp/testHCFS/data_%lld_%lld",
		this_inode, block_num);
#else
	sprintf(mock_block_path, "/tmp/testHCFS/data_%ld_%lld",
		this_inode, block_num);
#endif
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
		printf("Test: inode %d is updated\n", this_inode);
	}
	return 0;
}

int hcfs_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
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
	return 200;
}

int do_block_delete(ino_t this_inode, long long block_no, CURL_HANDLE *curl_handle)
{
	char deleteobjname[30];
#ifdef ARM_32bit_
	sprintf(deleteobjname, "data_%lld_%lld", this_inode, block_no);
#else
	sprintf(deleteobjname, "data_%ld_%lld", this_inode, block_no);
#endif
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
	long long ret_page_pos = sizeof(struct stat) + 
		sizeof(FILE_META_TYPE) + target_page * sizeof(BLOCK_ENTRY_PAGE);
	
	return ret_page_pos;
}

int write_log(int level, char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	//vprintf(format, alist);
	va_end(alist);
	return 0;
}

int hcfs_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
{
	long long sys_size, num_ino;

	if (no_backend_stat == TRUE)
		return 404;

	sys_size = 7687483;
	num_ino = 34334;
	fseek(fptr, 0, SEEK_SET);
	fwrite(&sys_size, sizeof(long long), 1, fptr);
	fwrite(&num_ino, sizeof(long long), 1, fptr);

	return 200;
}

