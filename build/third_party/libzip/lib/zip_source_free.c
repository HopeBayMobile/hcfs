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

#include "zipint.h"


ZIP_EXTERN void
zip_source_free(zip_source_t *src)
{
    if (src == NULL)
	return;

    if (src->refcount > 0) {
        src->refcount--;
    }
    if (src->refcount > 0) {
        return;
    }
    
    if (ZIP_SOURCE_IS_OPEN_READING(src)) {
	src->open_count = 1; /* force close */
	zip_source_close(src);
    }
    if (ZIP_SOURCE_IS_OPEN_WRITING(src)) {
        zip_source_rollback_write(src);
    }
    
    if (src->source_archive && !src->source_closed) {
        _zip_deregister_source(src->source_archive, src);
    }
    
    (void)_zip_source_call(src, NULL, 0, ZIP_SOURCE_FREE);
    
    if (src->src) {
        zip_source_free(src->src);
    }

    free(src);
}
