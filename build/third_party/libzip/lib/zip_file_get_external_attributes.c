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

#include "zipint.h"

int
zip_file_get_external_attributes(zip_t *za, zip_uint64_t idx, zip_flags_t flags, zip_uint8_t *opsys, zip_uint32_t *attributes)
{
    zip_dirent_t *de;

    if ((de=_zip_get_dirent(za, idx, flags, NULL)) == NULL)
	return -1;

    if (opsys)
	*opsys = (zip_uint8_t)((de->version_madeby >> 8) & 0xff);

    if (attributes)
	*attributes = de->ext_attrib;

    return 0;
}
