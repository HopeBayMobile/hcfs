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
zip_source_remove(zip_source_t *src)
{
    if (src->write_state == ZIP_SOURCE_WRITE_REMOVED) {
        return 0;
    }
    
    if (ZIP_SOURCE_IS_OPEN_READING(src)) {
	if (zip_source_close(src) < 0) {
	    return -1;
	}
    }
    if (src->write_state != ZIP_SOURCE_WRITE_CLOSED) {
        zip_source_rollback_write(src);
    }
    
    if (_zip_source_call(src, NULL, 0, ZIP_SOURCE_REMOVE) < 0) {
        return -1;
    }
    
    src->write_state = ZIP_SOURCE_WRITE_REMOVED;
    
    return 0;
}
