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
#include "mock_params.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include "global.h"
#include "meta.h"

#define TRUE 1
#define FALSE 0

int32_t fetch_block_path(char *pathname, ino_t this_inode, int64_t block_num)
{
	int32_t sub_dir;
	char tmpname[500];
	sub_dir = (this_inode + block_num) % NUMSUBDIR;
	sprintf(tmpname, "%s/sub_%d/block%" PRIu64 "_%lld", BLOCKPATH, sub_dir,
			(uint64_t)this_inode, block_num);
	strcpy(pathname, tmpname);
	return 0;
}

void init_mock_system_config()
{
	system_config->blockpath = malloc(sizeof(char) * 100);
	strcpy(BLOCKPATH, "testpatterns");
}

int32_t write_log(int32_t level, const char *format, ...)
{
	return 0;
}

int32_t get_block_dirty_status(char *path, FILE *fptr, char *status)
{
#ifdef _ANDROID_ENV_

	struct stat tmpstat;
	stat(path, &tmpstat);
		/* Use sticky bit to store dirty status */

	if ((tmpstat.st_mode & S_ISVTX) == 0)
		*status = FALSE;
	else
		*status = TRUE;
#else
	char tmpstr[5];
	getxattr(path, "user.dirty", (void *) tmpstr, 1);
	if (strncmp(tmpstr, "T", 1) == 0)
		*status = TRUE;
	else
		*status = FALSE;
#endif
	return 0;
}
