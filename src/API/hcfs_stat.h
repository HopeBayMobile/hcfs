/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfs_stat.h
* Abstract: This c header file for statistics operations.
*
* Revision History
* 2016/5/27 Modified after first code review.
*
**************************************************************************/

#ifndef GW20_HCFSAPI_STAT_H_
#define GW20_HCFSAPI_STAT_H_

#include <inttypes.h>

typedef struct {
	int64_t quota;
	int64_t vol_usage;
	int64_t cloud_usage;
	int64_t cache_total;
	int64_t cache_used;
	int64_t cache_dirty;
	int64_t pin_max;
	int64_t pin_total;
	int64_t xfer_up;
	int64_t xfer_down;
	int32_t cloud_stat;
	int32_t data_transfer;
	int64_t max_meta_size;
	int64_t meta_used_size;
} HCFS_STAT_TYPE;

int32_t get_hcfs_stat(HCFS_STAT_TYPE *hcfs_stats);

int32_t get_occupied_size(int64_t *occupied);

#endif  /* GW20_HCFSAPI_STAT_H_ */
