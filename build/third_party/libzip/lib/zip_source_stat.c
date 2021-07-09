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


ZIP_EXTERN int
zip_source_stat(zip_source_t *src, zip_stat_t *st)
{
    if (src->source_closed) {
        return -1;
    }
    if (st == NULL) {
        zip_error_set(&src->error, ZIP_ER_INVAL, 0);
	return -1;
    }

    zip_stat_init(st);
    
    if (ZIP_SOURCE_IS_LAYERED(src)) {
        if (zip_source_stat(src->src, st) < 0) {
            _zip_error_set_from_source(&src->error, src->src);
            return -1;
        }
    }

    if (_zip_source_call(src, st, sizeof(*st), ZIP_SOURCE_STAT) < 0) {
	return -1;
    }

    return 0;
}
