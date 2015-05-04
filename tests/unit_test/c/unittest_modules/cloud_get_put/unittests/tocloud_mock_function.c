#include <sys/stat.h>
#include "hcfs_tocloud.h"
#include "hcfs_clouddelete.h"
#include "params.h"
#include "mock_params.h"
#include "super_block.h"

SYSTEM_CONF_STRUCT system_config;

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	strcpy(pathname, "/tmp/mock_file_meta");
	return 0;
}

int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	char mock_block_path[50];
	FILE *ptr;
	sprintf(mock_block_path, "/tmp/mockblock_%d_%d", this_inode, block_num);
	strcpy(pathname, mock_block_path);
	return 0;
}

void con_object_dsync(DELETE_THREAD_TYPE *delete_thread_ptr)
{
	return;
}

int hcfs_init_backend(CURL_HANDLE *curl_handle)
{
	return HTTP_OK;
}

int super_block_update_transit(ino_t this_inode, char is_start_transit)
{
	return 0;
}

int hcfs_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
{
	return 0;
}

void do_block_delete(ino_t this_inode, long long block_no, CURL_HANDLE *curl_handle)
{
	return ;
}

int super_block_exclusive_locking(void)
{
	return 0;
}

int read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
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
