/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: utils.h
* Abstract: The header file for the utility functions for HCFS
*
* Revision History
* 2015/1/20 Jiahong added header for this file.
*
**************************************************************************/

#ifndef GW20_SRC_UTILS_H_
#define GW20_SRC_UTILS_H_

#include <sys/types.h>

/*BEGIN string utility definition*/
int fetch_meta_path(char *pathname, ino_t this_inode);   /*Will copy the filename of the meta file to pathname*/
int fetch_block_path(char *pathname, ino_t this_inode, long long block_num);   /*Will copy the filename of the block file to pathname*/
int parse_parent_self(const char *pathname, char *parentname, char *selfname);
int fetch_todelete_path(char *pathname, ino_t this_inode);   /*Will copy the filename of the meta file in todelete folder to pathname*/

/*END string utility definition*/


int read_system_config(char *config_path);
int validate_system_config();

off_t check_file_size(const char *path);
#endif  /* GW20_SRC_UTILS_H_ */
