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
zip_set_file_compression(zip_t *za, zip_uint64_t idx, zip_int32_t method, zip_uint32_t flags)
{
    zip_entry_t *e;
    zip_int32_t old_method;

    if (idx >= za->nentry) {
	zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return -1;
    }

    if (ZIP_IS_RDONLY(za)) {
	zip_error_set(&za->error, ZIP_ER_RDONLY, 0);
	return -1;
    }

    if (method != ZIP_CM_DEFAULT && method != ZIP_CM_STORE && method != ZIP_CM_DEFLATE) {
	zip_error_set(&za->error, ZIP_ER_COMPNOTSUPP, 0);
	return -1;
    }

    e = za->entry+idx;
    
    old_method = (e->orig == NULL ? ZIP_CM_DEFAULT : e->orig->comp_method);
    
    /* TODO: revisit this when flags are supported, since they may require a recompression */
    
    if (method == old_method) {
	if (e->changes) {
	    e->changes->changed &= ~ZIP_DIRENT_COMP_METHOD;
	    if (e->changes->changed == 0) {
		_zip_dirent_free(e->changes);
		e->changes = NULL;
	    }
	}
    }
    else {
        if (e->changes == NULL) {
            if ((e->changes=_zip_dirent_clone(e->orig)) == NULL) {
                zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
                return -1;
            }
        }

        e->changes->comp_method = method;
        e->changes->changed |= ZIP_DIRENT_COMP_METHOD;
    }
    
    return 0;
}
