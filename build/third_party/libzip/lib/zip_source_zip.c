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


#include <stdlib.h>
#include <string.h>

#include "zipint.h"


ZIP_EXTERN zip_source_t *
zip_source_zip(zip_t *za, zip_t *srcza, zip_uint64_t srcidx,
	       zip_flags_t flags, zip_uint64_t start, zip_int64_t len)
{
    if (len < -1) {
        zip_error_set(&za->error, ZIP_ER_INVAL, 0);
        return NULL;
    }
        
    if (len == -1)
	len = 0;
    
    if (start == 0 && len == 0)
	flags |= ZIP_FL_COMPRESSED;
    else
	flags &= ~ZIP_FL_COMPRESSED;

    return _zip_source_zip_new(za, srcza, srcidx, flags, start, (zip_uint64_t)len, NULL);
}
