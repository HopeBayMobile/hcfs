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

struct hcfs_stat { /* 128 bytes */
	uint64_t dev;
	uint64_t ino;
	uint32_t mode;
	uint32_t __pad1;
	uint64_t nlink; /* unsigned int in android */
	uint32_t uid;
	uint32_t gid;
	uint64_t rdev;
	int64_t size;
	int64_t blksize; /* int in android */
	int64_t blocks;
	int64_t	atime; /* use aarch64 time structure */
	uint64_t atime_nsec;
	int64_t	mtime;
	uint64_t mtime_nsec;
	int64_t	ctime;
	uint64_t ctime_nsec;
	uint32_t __unused4;
	uint32_t __unused5;
};
#endif /* SRC_HCFS_HCFS_STAT_H_ */
