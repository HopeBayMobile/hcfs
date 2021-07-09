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
#include <inttypes.h>
#include <stdlib.h>

#include "meta.h"

SYSTEM_CONF_STRUCT *system_config = NULL;
int32_t RETURN_PAGE_NOT_FOUND;
int32_t calloc_success;
int32_t fseek_success;

int32_t write_log(int32_t level, const char *format, ...)
{
	return 0;
}

int64_t seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr,
		int64_t target_page, int64_t hint_page)
{
	if (RETURN_PAGE_NOT_FOUND)
		return 0;
	else
		/* Linear page arrangement */
		return sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) +
			target_page * sizeof(BLOCK_ENTRY_PAGE);
}
