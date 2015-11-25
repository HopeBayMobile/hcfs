/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
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

extern SYSTEM_CONF_STRUCT system_config;

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

int read_system_config(char *config_path);
int validate_system_config(void);

off_t check_file_size(const char *path);

int change_system_meta(long long system_size_delta,
		long long cache_size_delta, long long cache_blocks_delta);

int set_block_dirty_status(char *path, FILE *fptr, char status);
int get_block_dirty_status(char *path, FILE *fptr, char *status);

/* Will copy the filename of ddt meta file to pathname */
int fetch_ddt_path(char *pathname, unsigned char last_char);

int fetch_error_download_path(char *path, ino_t inode);

void get_system_size(long long *cache_size, long long *pinned_size);

int update_file_stats(FILE *metafptr, long long num_blocks_delta,
			long long num_cached_blocks_delta,
			long long cached_size_delta);

inline int neg(int value);
#endif  /* SRC_HCFS_UTILS_H_ */
