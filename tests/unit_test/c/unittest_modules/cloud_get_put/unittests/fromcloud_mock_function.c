#include <sys/stat.h>
#include <stdio.h>
#include <curl/curl.h>
#include <semaphore.h>
#include "hcfscurl.h"
#include "mock_params.h"

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	strcpy(pathname, "/tmp/tmp_meta");
	return 0;
}

int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	strcpy(pathname, "/tmp/tmp_block");
	return 0;
}

int hcfs_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
{
	int inode, block_no;
	sscanf(objname, "data_%d_%d", &inode, &block_no);
	if (block_no == BLOCK_NO__FETCH_SUCCESS) {
		ftruncate(fileno(fptr), EXTEND_FILE_SIZE);
		return HTTP_OK;
	} else {
		return HTTP_FAIL;
	}
}

int sync_hcfs_system_data(char need_lock)
{
	return 0;
}
