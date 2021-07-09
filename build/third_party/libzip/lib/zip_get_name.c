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
zip_get_name(zip_t *za, zip_uint64_t idx, zip_flags_t flags)
{
    return _zip_get_name(za, idx, flags, &za->error);
}


const char *
_zip_get_name(zip_t *za, zip_uint64_t idx, zip_flags_t flags, zip_error_t *error)
{
    zip_dirent_t *de;
    const zip_uint8_t *str;

    if ((de=_zip_get_dirent(za, idx, flags, error)) == NULL)
	return NULL;

    if ((str=_zip_string_get(de->filename, NULL, flags, error)) == NULL)
	return NULL;

    return (const char *)str;
}
