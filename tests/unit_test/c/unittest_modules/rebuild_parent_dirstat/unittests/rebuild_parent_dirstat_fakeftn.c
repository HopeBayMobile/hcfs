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
#include "rebuild_parent_dirstat_unittest.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "fuseop.h"
#include "global.h"

int32_t write_log(int32_t level, const char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

int32_t fetch_meta_path(char *pathname, ino_t this_inode)
{
	sprintf(pathname, "%s_%" PRIu64 "", MOCK_META_PATH,
	        (uint64_t)this_inode);
	return 0;
}

int32_t lookup_add_parent(ino_t self_inode, ino_t parent_inode)
{
	fake_num_parents++;
	return 0;
}

int32_t fetch_all_parents(ino_t self_inode, int32_t *parentnum,
                          ino_t **parentlist)
{
	switch (self_inode) {
	case NO_PARENT_INO:
		*parentnum = 0;
		break;
	case ONE_PARENT_INO:
		*parentnum = 1;
		*parentlist = malloc(sizeof(ino_t) * (*parentnum));
		if (*parentlist == NULL)
			return -ENOMEM;
		(*parentlist)[0] = FAKE_EXIST_PARENT;
		break;
	default:
		*parentnum = 0;
		break;
	}
	return 0;
}

