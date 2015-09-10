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

#ifndef GW20_SRC_UTILS_H_
#define GW20_SRC_UTILS_H_

#include <sys/types.h>
#include <stdio.h>

/*BEGIN string utility definition*/

/*Will copy the filename of the meta file to pathname*/
int fetch_meta_path(char *pathname, ino_t this_inode);

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

int update_FS_statistics(char *pathname, long long system_size,
		long long num_inodes);

int read_FS_statistics(char *pathname, long long *system_size_ptr,
		long long *num_inodes_ptr);

int set_block_dirty_status(char *path, FILE *fptr, char status);
int get_block_dirty_status(char *path, FILE *fptr, char *status);

#endif  /* GW20_SRC_UTILS_H_ */
