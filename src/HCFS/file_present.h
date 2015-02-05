/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: file_present.h
* Abstract: The c header file for meta processing involving regular
*           files and directories in HCFS. "file_present" means 
*           "file-level presentation".
*
* Revision History
* 2015/2/5 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/

#ifndef GW20_HCFS_FILE_PRESENT_H_
#define GW20_HCFS_FILE_PRESENT_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int fetch_inode_stat(ino_t this_inode, struct stat *inode_stat);

int mknod_update_meta(ino_t self_inode, ino_t parent_inode, char *selfname,
						struct stat *this_stat);
int mkdir_update_meta(ino_t self_inode, ino_t parent_inode, char *selfname,
						struct stat *this_stat);
int unlink_update_meta(ino_t parent_inode, ino_t this_inode, char *selfname);

int meta_forget_inode(ino_t self_inode);

int rmdir_update_meta(ino_t parent_inode, ino_t this_inode, char *selfname);

#endif /* GW20_HCFS_FILE_PRESENT_H_ */
