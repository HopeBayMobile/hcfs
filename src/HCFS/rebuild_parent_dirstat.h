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
#ifndef GW20_HCFS_REBUILD_PARENT_DIRSTAT_H_
#define GW20_HCFS_REBUILD_PARENT_DIRSTAT_H_

#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <inttypes.h>
#include <unistd.h>

int32_t rebuild_parent_stat(ino_t this_inode, ino_t p_inode, int8_t d_type);

#endif  /* GW20_HCFS_REBUILD_PARENT_DIRSTAT_H_ */

