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


#define _ZIP_COMPILING_DEPRECATED
#include "zipint.h"


ZIP_EXTERN const char *
zip_get_file_comment(zip_t *za, zip_uint64_t idx, int *lenp, int flags)
{
    zip_uint32_t len;
    const char *s;

    if ((s=zip_file_get_comment(za, idx, &len, (zip_flags_t)flags)) != NULL) {
	if (lenp)
	    *lenp = (int)len;
    }

    return s;
}
