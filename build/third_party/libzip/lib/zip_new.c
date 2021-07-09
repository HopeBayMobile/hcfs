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


/* _zip_new:
   creates a new zipfile struct, and sets the contents to zero; returns
   the new struct. */

zip_t *
_zip_new(zip_error_t *error)
{
    zip_t *za;

    za = (zip_t *)malloc(sizeof(struct zip));
    if (!za) {
	zip_error_set(error, ZIP_ER_MEMORY, 0);
	return NULL;
    }

    if ((za->names = _zip_hash_new(ZIP_HASH_TABLE_SIZE, error)) == NULL) {
	free(za);
	return NULL;
    }

    za->src = NULL;
    za->open_flags = 0;
    zip_error_init(&za->error);
    za->flags = za->ch_flags = 0;
    za->default_password = NULL;
    za->comment_orig = za->comment_changes = NULL;
    za->comment_changed = 0;
    za->nentry = za->nentry_alloc = 0;
    za->entry = NULL;
    za->nopen_source = za->nopen_source_alloc = 0;
    za->open_source = NULL;
    za->tempdir = NULL;
    
    return za;
}
