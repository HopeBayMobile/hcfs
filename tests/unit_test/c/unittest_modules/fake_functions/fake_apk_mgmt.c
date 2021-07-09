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
#include <errno.h>
#include <string.h>

#include "macro.h"
#include "fake_misc.h"
int32_t toggle_use_minimal_apk(_UNUSED bool new_val) { return 0; }

int32_t initialize_minimal_apk(void) { return 0; }

int32_t terminate_minimal_apk(void) { return 0; }

int32_t create_minapk_table(void) { return 0; }

int32_t destroy_minapk_table(void) { return 0; }

int32_t insert_minapk_data(_UNUSED ino_t parent_ino,
			   _UNUSED const char *apk_name,
			   _UNUSED ino_t minapk_ino)
{
	return 0;
}

int32_t query_minapk_data(ino_t parent_ino,
			  const char *apk_name,
			  ino_t *minapk_ino)
{
	*minapk_ino = 0;
	if (parent_ino != TEST_APPDIR_INODE)
		return -ENOENT;

	if (strcmp(apk_name, "base.apk") != 0) {
		return -ENOENT;
	}
	if (cached_minapk == FALSE)
		return -ENOENT;

	if (exists_minapk == TRUE)
		*minapk_ino = TEST_APPMIN_INODE;
	return 0;
}

int32_t remove_minapk_data(_UNUSED ino_t parent_ino,
			   _UNUSED const char *apk_name)
{
	remove_apk_success = 1;
	strcpy(verified_apk_name, apk_name);
	return 0;
}
