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

