#include <sys/stat.h>
#include <stdio.h>
#include <curl/curl.h>
#include <semaphore.h>
#include "hcfscurl.h"
#include "params.h"
#include "super_block.h"

int hcfs_init_backend(CURL_HANDLE *curl_handle)
{
	return 0;
}


void hcfs_destroy_backend(CURL *curl)
{
	return;
}

int fetch_todelete_path(char *pathname, ino_t this_inode)
{
	return 0;
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
	return 0;
}

int super_block_share_locking(void)
{
	return 0;
}

int read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	return 0;
}

int super_block_share_release(void)
{
	return 0;
}
