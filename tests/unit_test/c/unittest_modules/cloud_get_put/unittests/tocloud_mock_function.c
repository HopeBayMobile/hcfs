#include <sys/stat.h>
#include "hcfs_tocloud.h"
#include "hcfs_clouddelete.h"
#include "params.h"
#include "mock_params.h"

SYSTEM_CONF_STRUCT system_config;

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	return 0;
}

int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
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
