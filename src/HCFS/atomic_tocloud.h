/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: atomic_tocloud.c
* Abstract: The c header file corresponding to atomic_tocloud.c.
*
* Revision History
* 2016/2/18 Kewei finish atomic upload and add revision history.
*
**************************************************************************/

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

#define GDRIVE_ID_LENGTH 64

/* Data that should be known by fuse process when uploading a file */
typedef struct {
	ino_t inode;
	BOOL is_uploading;
	BOOL is_revert;
	BOOL finish_sync;
	int32_t progress_list_fd;
} UPLOADING_COMMUNICATION_DATA;

/* Info entry for a specified block in uploading progress file */
typedef struct {
	char finish_uploading;
	char block_exist; /*first bit means toupload block, second means cloud*/
#if ENABLE(DEDUP)
	uint8_t to_upload_objid[OBJID_LENGTH];
	uint8_t backend_objid[OBJID_LENGTH];
#else
	char to_upload_gdrive_id[GDRIVE_ID_LENGTH];
	char backend_gdrive_id[GDRIVE_ID_LENGTH];
	int64_t to_upload_seq;
	int64_t backend_seq;
#endif
} BLOCK_UPLOADING_STATUS;

typedef struct {
	BLOCK_UPLOADING_STATUS status_entry[MAX_BLOCK_ENTRIES_PER_PAGE];
} BLOCK_UPLOADING_PAGE;


typedef struct {
	char now_action;
	int64_t backend_size;
	int64_t total_backend_blocks;
	int64_t toupload_size;
	int64_t total_toupload_blocks;
	int64_t direct;
	int64_t single_indirect;
	int64_t double_indirect;
	int64_t triple_indirect;
	int64_t quadruple_indirect;
} PROGRESS_META;

int32_t comm2fuseproc(ino_t this_inode, BOOL is_uploading,
	int32_t fd, BOOL is_revert, BOOL finish_sync);

int32_t get_progress_info(int32_t fd, int64_t block_index,
	BLOCK_UPLOADING_STATUS *block_uploading_status);

#if ENABLE(DEDUP)
int32_t set_progress_info(int32_t fd, int64_t block_index,
	const char *toupload_exist, const char *backend_exist,
	const uint8_t *toupload_objid, const uint8_t *backend_objid,
	const char *finish);
#else
int32_t set_progress_info(int32_t fd, int64_t block_index,
	const char *toupload_exist, const char *backend_exist,
	const int64_t *toupload_seq, const int64_t *backend_seq,
	const char *toupload_gdrive_id, const char *backend_gdrive_id,
	const char *finish);
#endif

int32_t init_progress_info(int32_t fd, int64_t backend_blocks, int64_t backend_size,
	FILE *backend_metafptr, uint8_t *last_pin_status);

int32_t create_progress_file(ino_t inode);

int32_t del_progress_file(int32_t fd, ino_t inode);

int32_t check_and_copy_file(const char *srcpath, const char *tarpath,
		BOOL lock_src, BOOL reject_if_nospc);

int32_t fetch_toupload_meta_path(char *pathname, ino_t inode);

int32_t fetch_toupload_block_path(char *pathname, ino_t inode,
	int64_t block_no, int64_t seq);

int32_t fetch_backend_meta_path(char *pathname, ino_t inode);
void fetch_del_backend_meta_path(char *pathname, ino_t inode);

char block_finish_uploading(int32_t fd, int64_t blockno);

int64_t query_status_page(int32_t fd, int64_t block_index);

int32_t init_backend_file_info(const SYNC_THREAD_TYPE *ptr,
		int64_t *backend_size, int64_t *total_backend_blocks,
		int64_t upload_seq, uint8_t *last_pin_status);

void continue_inode_sync(SYNC_THREAD_TYPE *data_ptr);

int32_t change_action(int32_t fd, char new_action);

void fetch_progress_file_path(char *pathname, ino_t inode);

#endif
