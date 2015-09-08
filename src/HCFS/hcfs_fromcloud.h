/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfs_fromcloud.h
* Abstract: The c header file for retrieving meta or data from backend.
*
* Revision History
* 2015/2/12 Jiahong created this file (moved part of code from hcfscurl.h)
* 2015/2/12 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/
#ifndef GW20_HCFS_HCFS_FROMCLOUD_H_
#define GW20_HCFS_HCFS_FROMCLOUD_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "fuseop.h"

typedef struct {
	ino_t this_inode;
	long long block_no;
	off_t page_start_fpos;
	int entry_index;
} PREFETCH_STRUCT_TYPE;

pthread_attr_t prefetch_thread_attr;
void prefetch_block(PREFETCH_STRUCT_TYPE *ptr);
int fetch_from_cloud(FILE *fptr,
#if (DEDUP_ENABLE)
		unsigned char *blk_hash);
#else
		ino_t this_inode, long long block_no);
#endif

#endif  /* GW20_HCFS_HCFS_FROMCLOUD_H_ */
