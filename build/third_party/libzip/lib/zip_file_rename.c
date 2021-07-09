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


ZIP_EXTERN int
zip_file_rename(zip_t *za, zip_uint64_t idx, const char *name, zip_flags_t flags)
{
    const char *old_name;
    int old_is_dir, new_is_dir;
    
    if (idx >= za->nentry || (name != NULL && strlen(name) > ZIP_UINT16_MAX)) {
	zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return -1;
    }

    if (ZIP_IS_RDONLY(za)) {
	zip_error_set(&za->error, ZIP_ER_RDONLY, 0);
	return -1;
    }

    if ((old_name=zip_get_name(za, idx, 0)) == NULL)
	return -1;
								    
    new_is_dir = (name != NULL && name[strlen(name)-1] == '/');
    old_is_dir = (old_name[strlen(old_name)-1] == '/');

    if (new_is_dir != old_is_dir) {
	zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return -1;
    }

    return _zip_set_name(za, idx, name, flags);
}
