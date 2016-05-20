/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: lookup_count.h
* Abstract: The c header file for types in lookup count table (for inodes).
*
* Revision History
* 2015/8/4 Jiahong moves the types from lookup_count.h to here
*
**************************************************************************/
#ifndef GW20_HCFS_LOOKUP_COUNT_TYPES_H_
#define GW20_HCFS_LOOKUP_COUNT_TYPES_H_

#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_LOOKUP_ENTRIES 65536

typedef struct lookup_template {
	ino_t this_inode;
	int32_t lookup_count;
	char to_delete;
	char d_type;
	struct lookup_template *next;
} LOOKUP_NODE_TYPE;

typedef struct {
	sem_t entry_sem;
	LOOKUP_NODE_TYPE *head;
} LOOKUP_HEAD_TYPE;

#endif  /* GW20_HCFS_LOOKUP_COUNT_TYPES_H_ */

