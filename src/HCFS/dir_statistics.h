/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: dir_statistics.h
* Abstract: The header file for keeping track the number of decendants that
*           are local or cloud or hybrid
*
* Revision History
* 2015/11/11 Jiahong created this file
*
**************************************************************************/
#ifndef GW20_HCFS_DIR_STATISTICS_H_
#define GW20_HCFS_DIR_STATISTICS_H_

#include <stdio.h>
#include <sys/types.h>

/* The structure for keeping statistics for a directory */
typedef struct {
	int64_t num_local;
	int64_t num_cloud;
	int64_t num_hybrid;
} DIR_STATS_TYPE;

/* Share with path lookup the same resource lock */
FILE *dirstat_lookup_data_fptr;

int32_t init_dirstat_lookup(void);
void destroy_dirstat_lookup(void);

int32_t reset_dirstat_lookup(ino_t thisinode);
int32_t update_dirstat_file(ino_t thisinode, DIR_STATS_TYPE *newstat);
int32_t update_dirstat_parent(ino_t baseinode, DIR_STATS_TYPE *newstat);
int32_t read_dirstat_lookup(ino_t thisinode, DIR_STATS_TYPE *newstat);

#endif  /* GW20_HCFS_DIR_STATISTICS_H_ */

