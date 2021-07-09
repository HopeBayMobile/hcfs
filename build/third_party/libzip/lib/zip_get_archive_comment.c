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


#include <string.h>

#include "zipint.h"


ZIP_EXTERN const char *
zip_get_archive_comment(zip_t *za, int *lenp, zip_flags_t flags)
{
    zip_string_t *comment;
    zip_uint32_t len;
    const zip_uint8_t *str;

    if ((flags & ZIP_FL_UNCHANGED) || (za->comment_changes == NULL))
	comment = za->comment_orig;
    else
	comment = za->comment_changes;

    if ((str=_zip_string_get(comment, &len, flags, &za->error)) == NULL)
	return NULL;

    if (lenp)
	*lenp = (int)len;

    return (const char *)str;
}
