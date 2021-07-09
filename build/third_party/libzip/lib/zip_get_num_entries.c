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


ZIP_EXTERN zip_int64_t
zip_get_num_entries(zip_t *za, zip_flags_t flags)
{
    zip_uint64_t n;

    if (za == NULL)
	return -1;

    if (flags & ZIP_FL_UNCHANGED) {
	n = za->nentry;
	while (n>0 && za->entry[n-1].orig == NULL)
	    --n;
	return (zip_int64_t)n;
    }
    return (zip_int64_t)za->nentry;
}
