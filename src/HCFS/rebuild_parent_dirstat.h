/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: rebuild_parent_dirstat.h
* Abstract: The header file for rebuilding parent lookup dbs and dir
*           statistics
*
* Revision History
* 2016/6/1 Jiahong created this file
*
**************************************************************************/
#ifndef GW20_HCFS_REBUILD_PARENT_DIRSTAT_H_
#define GW20_HCFS_REBUILD_PARENT_DIRSTAT_H_

#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <inttypes.h>
#include <unistd.h>

int32_t rebuild_parent_stat(ino_t this_inode, ino_t p_inode, int8_t d_type);

#endif  /* GW20_HCFS_REBUILD_PARENT_DIRSTAT_H_ */

