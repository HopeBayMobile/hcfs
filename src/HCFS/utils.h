/*************************************************************************
*
* Copyright © 2014-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: utils.h
* Abstract: The header file for the utility functions for HCFS
*
* Revision History
* 2015/1/20 Jiahong added header for this file.
* 2015/2/11 Jiahong revised coding style.
*
**************************************************************************/

#ifndef SRC_HCFS_UTILS_H_
#define SRC_HCFS_UTILS_H_

#include <sys/types.h>
#include <stdio.h>
#include "params.h"

extern SYSTEM_CONF_STRUCT *system_config;

#include "dir_statistics.h"
#include "global.h"

int32_t check_and_create_metapaths(void);
int32_t check_and_create_blockpaths(void);

/*BEGIN string utility definition*/

/*Will copy the filename of the meta file to pathname*/
int32_t fetch_meta_path(char *pathname, ino_t this_inode);

int32_t fetch_stat_path(char *pathname, ino_t this_inode);

int32_t fetch_trunc_path(char *pathname, ino_t this_inode);

/*Will copy the filename of the block file to pathname*/
int32_t fetch_block_path(char *pathname, ino_t this_inode, int64_t block_num);
int32_t parse_parent_self(const char *pathname, char *parentname, char *selfname);

/*Will copy the filename of the meta file in todelete folder to pathname*/
int32_t fetch_todelete_path(char *pathname, ino_t this_inode);

/*END string utility definition*/

int32_t read_system_config(const char *config_path, SYSTEM_CONF_STRUCT *config);
int32_t validate_system_config(SYSTEM_CONF_STRUCT *config);
int32_t init_cache_thresholds(SYSTEM_CONF_STRUCT *config);
int32_t init_system_config_settings(const char *config_path,
				    SYSTEM_CONF_STRUCT *config);

off_t check_file_size(const char *path);

int32_t change_system_meta(int64_t system_data_size_delta,
	int64_t meta_size_delta, int64_t cache_data_size_delta,
	int64_t cache_blocks_delta, int64_t dirty_cache_delta,
	int64_t unpin_dirty_data_size, BOOL need_sync);

void _shift_xfer_window(void);
int32_t change_xfer_meta(int64_t xfer_size_upload, int64_t xfer_size_download,
		int64_t xfer_throughput, int64_t xfer_total_obj);

int32_t update_fs_backend_usage(FILE *fptr, int64_t fs_total_size_delta,
		int64_t fs_meta_size_delta, int64_t fs_num_inodes_delta);

int32_t update_backend_usage(int64_t total_backend_size_delta,
		int64_t meta_size_delta, int64_t num_inodes_delta);

int32_t set_block_dirty_status(char *path, FILE *fptr, char status);
int32_t get_block_dirty_status(char *path, FILE *fptr, char *status);

void fetch_backend_block_objname(char *objname,
#if DEDUP_ENABLE
	unsigned char *obj_id);
#else
	ino_t inode, long long block_no, long long seqnum);
#endif

void fetch_backend_meta_objname(char *objname, ino_t inode);

/* Will copy the filename of ddt meta file to pathname */
int32_t fetch_ddt_path(char *pathname, uint8_t last_char);

int32_t fetch_error_download_path(char *path, ino_t inode);

void get_system_size(int64_t *cache_size, int64_t *pinned_size);

int32_t update_sb_size(void);

int32_t update_file_stats(FILE *metafptr, int64_t num_blocks_delta,
			int64_t num_cached_blocks_delta,
			int64_t cached_size_delta,
			int64_t dirty_data_size_delta,
			ino_t thisinode);
/* Function for checking if a file is local, cloud, or hybrid */
int32_t check_file_storage_location(FILE *fptr,  DIR_STATS_TYPE *newstat);

int32_t reload_system_config(const char *config_path);

void nonblock_sleep(uint32_t secs, BOOL (*wakeup_condition)(void));

int32_t ignore_sigpipe(void);

BOOL is_natural_number(char *str);

int32_t get_meta_size(ino_t inode, int64_t *metasize);

int32_t get_quota_from_backup(int64_t *quota);
int32_t meta_nospc_log(const char *func_name, int32_t lines);
#endif  /* SRC_HCFS_UTILS_H_ */
