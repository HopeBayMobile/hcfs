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

ZIP_EXTERN int zip_file_set_mtime(zip_t *za, zip_uint64_t idx, time_t mtime, zip_flags_t flags)
{
    zip_entry_t *e;
    int changed;
    
    if (_zip_get_dirent(za, idx, 0, NULL) == NULL)
        return -1;
    
    if (ZIP_IS_RDONLY(za)) {
        zip_error_set(&za->error, ZIP_ER_RDONLY, 0);
        return -1;
    }
    
    e = za->entry+idx;

    changed = e->orig == NULL || mtime != e->orig->last_mod;
    
    if (changed) {
        if (e->changes == NULL) {
            if ((e->changes=_zip_dirent_clone(e->orig)) == NULL) {
                zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
                return -1;
            }
        }
        e->changes->last_mod = mtime;
        e->changes->changed |= ZIP_DIRENT_LAST_MOD;
    }
    else {
        if (e->changes) {
            e->changes->changed &= ~ZIP_DIRENT_LAST_MOD;
            if (e->changes->changed == 0) {
		_zip_dirent_free(e->changes);
                e->changes = NULL;
            }
        }
    }
    
    return 0;
}
