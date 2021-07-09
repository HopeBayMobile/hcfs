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
zip_file_replace(zip_t *za, zip_uint64_t idx, zip_source_t *source, zip_flags_t flags)
{
    if (idx >= za->nentry || source == NULL) {
	zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return -1;
    }

    if (_zip_file_replace(za, idx, NULL, source, flags) == -1)
	return -1;

    return 0;
}



/* NOTE: Signed due to -1 on error.  See zip_add.c for more details. */

zip_int64_t
_zip_file_replace(zip_t *za, zip_uint64_t idx, const char *name, zip_source_t *source, zip_flags_t flags)
{
    zip_uint64_t za_nentry_prev;
    
    if (ZIP_IS_RDONLY(za)) {
	zip_error_set(&za->error, ZIP_ER_RDONLY, 0);
	return -1;
    }

    za_nentry_prev = za->nentry;
    if (idx == ZIP_UINT64_MAX) {
	zip_int64_t i = -1;
	
	if (flags & ZIP_FL_OVERWRITE)
	    i = _zip_name_locate(za, name, flags, NULL);

	if (i == -1) {
	    /* create and use new entry, used by zip_add */
	    if ((i=_zip_add_entry(za)) < 0)
		return -1;
	}
	idx = (zip_uint64_t)i;
    }
    
    if (name && _zip_set_name(za, idx, name, flags) != 0) {
	if (za->nentry != za_nentry_prev) {
	    _zip_entry_finalize(za->entry+idx);
	    za->nentry = za_nentry_prev;
	}
	return -1;
    }

    /* does not change any name related data, so we can do it here;
     * needed for a double add of the same file name */
    _zip_unchange_data(za->entry+idx);

    if (za->entry[idx].orig != NULL && (za->entry[idx].changes == NULL || (za->entry[idx].changes->changed & ZIP_DIRENT_COMP_METHOD) == 0)) {
        if (za->entry[idx].changes == NULL) {
            if ((za->entry[idx].changes=_zip_dirent_clone(za->entry[idx].orig)) == NULL) {
                zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
                return -1;
            }
        }

        za->entry[idx].changes->comp_method = ZIP_CM_REPLACED_DEFAULT;
        za->entry[idx].changes->changed |= ZIP_DIRENT_COMP_METHOD;
    }
	
    za->entry[idx].source = source;

    return (zip_int64_t)idx;
}
