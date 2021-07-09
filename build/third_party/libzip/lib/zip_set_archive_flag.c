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
zip_set_archive_flag(zip_t *za, zip_flags_t flag, int value)
{
    unsigned int new_flags;
    
    if (value)
	new_flags = za->ch_flags | flag;
    else
	new_flags = za->ch_flags & ~flag;

    if (new_flags == za->ch_flags)
	return 0;

    if (ZIP_IS_RDONLY(za)) {
	zip_error_set(&za->error, ZIP_ER_RDONLY, 0);
	return -1;
    }

    if ((flag & ZIP_AFL_RDONLY) && value
	&& (za->ch_flags & ZIP_AFL_RDONLY) == 0) {
	if (_zip_changed(za, NULL)) {
	    zip_error_set(&za->error, ZIP_ER_CHANGED, 0);
	    return -1;
	}
    }

    za->ch_flags = new_flags;

    return 0;
}
