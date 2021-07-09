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

#ifndef GW20_HCFS_DO_FALLOCATE_H_
#define GW20_HCFS_DO_FALLOCATE_H_

#include <sys/types.h>
#include <fuse/fuse_lowlevel.h>

#include "meta_mem_cache.h"

int32_t do_fallocate(ino_t this_inode,
		     HCFS_STAT *newstat,
		     int32_t mode,
		     off_t offset,
		     off_t length,
		     META_CACHE_ENTRY_STRUCT **body_ptr,
		     fuse_req_t req);

#endif  /* GW20_HCFS_DO_FALLOCATE_H_ */
