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

