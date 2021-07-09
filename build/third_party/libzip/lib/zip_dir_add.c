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


/* NOTE: Signed due to -1 on error.  See zip_add.c for more details. */

ZIP_EXTERN zip_int64_t
zip_dir_add(zip_t *za, const char *name, zip_flags_t flags)
{
    size_t len;
    zip_int64_t idx;
    char *s;
    zip_source_t *source;

    if (ZIP_IS_RDONLY(za)) {
	zip_error_set(&za->error, ZIP_ER_RDONLY, 0);
	return -1;
    }

    if (name == NULL) {
	zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return -1;
    }

    s = NULL;
    len = strlen(name);

    if (name[len-1] != '/') {
	if ((s=(char *)malloc(len+2)) == NULL) {
	    zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
	    return -1;
	}
	strcpy(s, name);
	s[len] = '/';
	s[len+1] = '\0';
    }

    if ((source=zip_source_buffer(za, NULL, 0, 0)) == NULL) {
	free(s);
	return -1;
    }
	
    idx = _zip_file_replace(za, ZIP_UINT64_MAX, s ? s : name, source, flags);

    free(s);

    if (idx < 0)
	zip_source_free(source);
    else {
	if (zip_file_set_external_attributes(za, (zip_uint64_t)idx, 0, ZIP_OPSYS_DEFAULT, ZIP_EXT_ATTRIB_DEFAULT_DIR) < 0) {
	    zip_delete(za, (zip_uint64_t)idx);
	    return -1;
	}
    }

    return idx;
}
