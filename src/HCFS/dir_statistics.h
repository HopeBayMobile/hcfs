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

