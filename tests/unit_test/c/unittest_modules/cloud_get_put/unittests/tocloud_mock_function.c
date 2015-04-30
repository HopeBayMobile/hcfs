#include <sys/stat.h>
#include "hcfs_clouddelete.h"

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
