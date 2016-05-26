/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: lookup_count.h
* Abstract: The c header file for the lookup count table (for inodes).
*
* Revision History
* 2015/5/15 Jiahong created the file.
* 2015/8/4 Jiahong splited the types to lookup_count_types.h
*
**************************************************************************/
#ifndef GW20_HCFS_LOOKUP_COUNT_H_
#define GW20_HCFS_LOOKUP_COUNT_H_

#include "lookup_count_types.h"
#include "mount_manager.h"

int32_t lookup_init(LOOKUP_HEAD_TYPE *lookup_table);
int32_t lookup_increase(LOOKUP_HEAD_TYPE *lookup_table, ino_t this_inode,
				int32_t amount, char d_type);
int32_t lookup_decrease(LOOKUP_HEAD_TYPE *lookup_table, ino_t this_inode,
			int32_t amount, char *d_type, char *need_delete);
int32_t lookup_markdelete(LOOKUP_HEAD_TYPE *lookup_table, ino_t this_inode);

int32_t lookup_destroy(LOOKUP_HEAD_TYPE *lookup_table, MOUNT_T *tmpptr);

#endif  /* GW20_HCFS_LOOKUP_COUNT_H_ */

