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
#include <sys/stat.h>
#include "mock_param.h"
#include "global.h"

int32_t actual_delete_inode(ino_t this_inode, char d_type)
{
	check_actual_delete_table[this_inode] = TRUE;
	return 0;
}

int32_t disk_checkdelete(ino_t this_inode)
{
	return 1;
}

int32_t write_log(int32_t level, const char *format, ...)
{
	return 0;
}

