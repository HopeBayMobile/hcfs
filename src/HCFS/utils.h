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

/*BEGIN string utility definition*/

/*Will copy the filename of the meta file to pathname*/
int fetch_meta_path(char *pathname, ino_t this_inode);

int fetch_stat_path(char *pathname, ino_t this_inode);

int fetch_trunc_path(char *pathname, ino_t this_inode);

/*Will copy the filename of the block file to pathname*/
int fetch_block_path(char *pathname, ino_t this_inode, long long block_num);
int parse_parent_self(const char *pathname, char *parentname, char *selfname);

/*Will copy the filename of the meta file in todelete folder to pathname*/
int fetch_todelete_path(char *pathname, ino_t this_inode);

/*END string utility definition*/

int read_system_config(const char *config_path, SYSTEM_CONF_STRUCT *config);
int validate_system_config(SYSTEM_CONF_STRUCT *config);

off_t check_file_size(const char *path);

int change_system_meta(long long system_data_size_delta,
	long long meta_size_delta, long long cache_data_size_delta,
	long long cache_blocks_delta, long long dirty_cache_delta,
	long long unpin_dirty_data_size, BOOL need_sync);

int update_fs_backend_usage(FILE *fptr, long long fs_total_size_delta,
		long long fs_meta_size_delta, long long fs_num_inodes_delta);

int update_backend_usage(long long total_backend_size_delta,
		long long meta_size_delta, long long num_inodes_delta);

int set_block_dirty_status(char *path, FILE *fptr, char status);
int get_block_dirty_status(char *path, FILE *fptr, char *status);

void fetch_backend_block_objname(char *objname,
#if DEDUP_ENABLE
	unsigned char *obj_id);
#else
	ino_t inode, long long block_no, long long seqnum);
#endif

void fetch_backend_meta_objname(char *objname, ino_t inode);

/* Will copy the filename of ddt meta file to pathname */
int fetch_ddt_path(char *pathname, unsigned char last_char);

int fetch_error_download_path(char *path, ino_t inode);

void get_system_size(long long *cache_size, long long *pinned_size);

int update_sb_size();

int update_file_stats(FILE *metafptr, long long num_blocks_delta,
			long long num_cached_blocks_delta,
			long long cached_size_delta,
			long long dirty_data_size_delta,
			ino_t thisinode);
/* Function for checking if a file is local, cloud, or hybrid */
int check_file_storage_location(FILE *fptr,  DIR_STATS_TYPE *newstat);

int reload_system_config(const char *config_path);

void nonblock_sleep(unsigned int secs, BOOL (*wakeup_condition)());

int ignore_sigpipe(void);

BOOL is_natural_number(char *str);

int get_meta_size(ino_t inode, long long *metasize);

int get_quota_from_backup(long long *quota);
#endif  /* SRC_HCFS_UTILS_H_ */
