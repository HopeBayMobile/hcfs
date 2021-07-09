/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
