#ifndef GW20_HCFS_ATOMIC_TOCLOUD_H_
#define GW20_HCFS_ATOMIC_TOCLOUD_H_

#include "hcfs_tocloud.h"
#include <sys/un.h>
#include <sys/file.h>

#define TOUPLOAD_BLOCK_EXIST(flag) (((flag) & 1) == 1 ? TRUE : FALSE)
#define SET_TOUPLOAD_BLOCK_EXIST(flag) ((flag) |= 1)

#define CLOUD_BLOCK_EXIST(flag) (((flag) & 2) == 2 ? TRUE : FALSE)
#define SET_CLOUD_BLOCK_EXIST(flag) ((flag) |= 2)

/* Data that should be known by fuse process when uploading a file */
typedef struct {
	ino_t inode;
	char is_uploading;
	int progress_list_fd;
} UPLOADING_COMMUNICATION_DATA;

typedef struct {
	char finish_uploading;
	char block_exist; /*first bit means toupload block, second means cloud*/
#ifdef DEDUP_ENABLE
	unsigned char to_upload_objid[OBJID_LENGTH];
	unsigned char backend_objid[OBJID_LENGTH];
#else
	long long to_upload_seq;
	long long backend_seq;
#endif
} BLOCK_UPLOADING_STATUS;


int tag_status_on_fuse(ino_t this_inode, char status, int fd);

int get_progress_info(int fd, long long block_index,
	BLOCK_UPLOADING_STATUS *block_uploading_status);

int get_progress_info_nonlock(int fd, long long block_index,
	BLOCK_UPLOADING_STATUS *block_uploading_status);

int set_progress_info(int fd, long long block_index,
	BLOCK_UPLOADING_STATUS *set_block_uploading_status,
	char set_which_one);

int init_progress_info(int fd, long long backend_blocks,
	FILE *backend_metafptr);

int open_progress_info(ino_t inode);

int close_progress_info(int fd, ino_t inode);

int check_and_copy_file(const char *srcpath, const char *tarpath);

int fetch_toupload_meta_path(char *pathname, ino_t inode);

int fetch_toupload_block_path(char *pathname, ino_t inode,
	long long block_no, long long seq);

int fetch_backend_meta_path(char *pathname, ino_t inode);

char did_block_finish_uploading(int fd, long long blockno);

#endif
