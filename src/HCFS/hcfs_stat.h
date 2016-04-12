/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: common_stat.h
* Abstract: The header file for common stat definition
*
* Revision History
* 2016/3/22 Jethro add common_stat.c and common_stat.h
*
**************************************************************************/

#ifndef SRC_HCFS_HCFS_STAT_H_
#define SRC_HCFS_HCFS_STAT_H_

#include <inttypes.h>
#include <time.h>

struct hcfs_stat {
	uint64_t st_dev;
	uint64_t st_ino;
	uint32_t st_mode;
	uint64_t st_nlink; /* unsigned int in android */
	uint32_t st_uid;
	uint32_t st_gid;
	uint64_t st_rdev;
	int64_t st_size;
	int64_t st_blksize; /* int in android */
	int64_t st_blocks;
	/* use aarch64's time structure */
	int64_t	st_atime;
	uint64_t st_atime_nsec;
	int64_t	st_mtime;
	uint64_t st_mtime_nsec;
	int64_t	st_ctime;
	uint64_t st_ctime_nsec;
};

#endif /* SRC_HCFS_HCFS_STAT_H_ */
