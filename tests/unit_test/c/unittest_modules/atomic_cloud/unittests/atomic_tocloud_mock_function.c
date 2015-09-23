#include "hcfs_tocloud.h"

int write_log(int level, char *format, ...)
{
	return 0;
}

void sync_single_inode(SYNC_THREAD_TYPE *ptr)
{
	return;
}

int delete_backend_blocks(int progress_fd, long long total_blocks, ino_t inode,
		char delete_which_one)
{
	return 0;
}

long long seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr,
		long long target_page, long long hint_page)
{
	return 0;
}
