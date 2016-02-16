#ifndef GW20_HCFS_ATOMIC_TOCLOUD_H_
#define GW20_HCFS_ATOMIC_TOCLOUD_H_

#include "hcfs_tocloud.h"
#include <sys/un.h>
#include <sys/file.h>

#define TOUPLOAD_BLOCK_EXIST(flag) (((flag) & 1) == 1 ? TRUE : FALSE)
#define SET_TOUPLOAD_BLOCK_EXIST(flag) ((flag) |= 1)

#define CLOUD_BLOCK_EXIST(flag) (((flag) & 2) == 2 ? TRUE : FALSE)
#define SET_CLOUD_BLOCK_EXIST(flag) ((flag) |= 2)

#define PREPARING 0
#define NOW_UPLOADING 1
#define DEL_TOUPLOAD_BLOCKS 2
#define DEL_BACKEND_BLOCKS 3

/* Data that should be known by fuse process when uploading a file */
typedef struct {
	ino_t inode;
	char is_uploading;
	BOOL is_revert;
	BOOL finish_sync;
	int progress_list_fd;
} UPLOADING_COMMUNICATION_DATA;

/* Info entry for a specified block in uploading progress file */
typedef struct {
	char finish_uploading;
	char block_exist; /*first bit means toupload block, second means cloud*/
#if (DEDUP_ENABLE)
	unsigned char to_upload_objid[OBJID_LENGTH];
	unsigned char backend_objid[OBJID_LENGTH];
#else
	long long to_upload_seq;
	long long backend_seq;
#endif
} BLOCK_UPLOADING_STATUS;

typedef struct {
	BLOCK_UPLOADING_STATUS status_entry[MAX_BLOCK_ENTRIES_PER_PAGE];
} BLOCK_UPLOADING_PAGE;


typedef struct {
	char now_action;
	long long backend_size;
	long long total_backend_blocks;
	long long toupload_size;
	long long total_toupload_blocks;
	long long direct;
	long long single_indirect;
	long long double_indirect;
	long long triple_indirect;
	long long quadruple_indirect;
} PROGRESS_META;

int comm2fuseproc(ino_t this_inode, BOOL is_uploading,
	int fd, BOOL is_revert, BOOL finish_sync);

int get_progress_info(int fd, long long block_index,
	BLOCK_UPLOADING_STATUS *block_uploading_status);

#if (DEDUP_ENABLE)
int set_progress_info(int fd, long long block_index,
	const char *toupload_exist, const char *backend_exist,
	const unsigned char *toupload_objid, const unsigned char *backend_objid,
	const char *finish);
#else
int set_progress_info(int fd, long long block_index,
	const char *toupload_exist, const char *backend_exist,
	const long long *toupload_seq, const long long *backend_seq,
	const char *finish);
#endif

int init_progress_info(int fd, long long backend_blocks, long long backend_size,
	FILE *backend_metafptr);

int create_progress_file(ino_t inode);

int del_progress_file(int fd, ino_t inode);

int check_and_copy_file(const char *srcpath, const char *tarpath,
		BOOL lock_src);

int fetch_toupload_meta_path(char *pathname, ino_t inode);

int fetch_toupload_block_path(char *pathname, ino_t inode,
	long long block_no, long long seq);

int fetch_backend_meta_path(char *pathname, ino_t inode);
void fetch_del_backend_meta_path(char *pathname, ino_t inode);

char did_block_finish_uploading(int fd, long long blockno);

long long query_status_page(int fd, long long block_index);

void continue_inode_upload(SYNC_THREAD_TYPE *data_ptr);

int init_backend_file_info(const SYNC_THREAD_TYPE *ptr, long long *backend_size,
		long long *total_backend_blocks);

void continue_inode_sync(SYNC_THREAD_TYPE *data_ptr);

int change_action(int fd, char now_action);

void fetch_progress_file_path(char *pathname, ino_t inode);

#endif
