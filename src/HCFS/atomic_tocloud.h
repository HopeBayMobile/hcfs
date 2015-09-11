#ifndef GW20_HCFS_ATOMIC_TOCLOUD_H_
#define GW20_HCFS_ATOMIC_TOCLOUD_H_

#include "hcfs_tocloud.h"
#include <sys/un.h>
#include <sys/file.h>

/* Data that should be known by fuse process when uploading a file */
typedef struct {
	ino_t inode;
	char is_uploading;
	int progress_list_fd;
} UPLOADING_COMMUNICATION_DATA;

typedef struct {
	char finish_uploading;
	long long to_upload_seq;
	long long backend_seq;
} BLOCK_UPLOADING_STATUS;

int tag_status_on_fuse(ino_t this_inode, char status, int fd);

int get_progress_info(int fd, long long block_index,
	BLOCK_UPLOADING_STATUS *block_uploading_status);

int get_progress_info_nonlock(int fd, long long block_index,
	BLOCK_UPLOADING_STATUS *block_uploading_status);

int set_progress_info(int fd, long long block_index, char finish_uploading,
	long long to_upload_seq, long long backend_seq);

int init_progress_info(int fd, long long num_block, FILE *backend_metafptr);

int open_progress_info(ino_t inode);

int close_progress_info(int fd, ino_t inode);

int check_and_copy_file(const char *srcpath, const char *tarpath);

int fetch_toupload_block_path(char *pathname, ino_t inode,
	long long block_no, long long seq);

#endif
