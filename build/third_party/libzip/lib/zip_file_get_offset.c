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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "zipint.h"


/* _zip_file_get_offset(za, ze):
   Returns the offset of the file data for entry ze.

   On error, fills in za->error and returns 0.
*/

zip_uint64_t
_zip_file_get_offset(const zip_t *za, zip_uint64_t idx, zip_error_t *error)
{
    zip_uint64_t offset;
    zip_int32_t size;

    offset = za->entry[idx].orig->offset;

    if (zip_source_seek(za->src, (zip_int64_t)offset, SEEK_SET) < 0) {
	_zip_error_set_from_source(error, za->src);
	return 0;
    }

    /* TODO: cache? */
    if ((size=_zip_dirent_size(za->src, ZIP_EF_LOCAL, error)) < 0)
	return 0;

    if (offset+(zip_uint32_t)size > ZIP_INT64_MAX) {
        zip_error_set(error, ZIP_ER_SEEK, EFBIG);
        return 0;
    }
    
    return offset + (zip_uint32_t)size;
}
