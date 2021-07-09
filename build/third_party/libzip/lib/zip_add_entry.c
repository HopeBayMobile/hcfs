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


/* NOTE: Signed due to -1 on error.  See zip_add.c for more details. */

zip_int64_t
_zip_add_entry(zip_t *za)
{
    zip_uint64_t idx;

    if (za->nentry+1 >= za->nentry_alloc) {
	zip_entry_t *rentries;
	zip_uint64_t nalloc = za->nentry_alloc;
	zip_uint64_t additional_entries = 2 * nalloc;
	zip_uint64_t realloc_size;

	if (additional_entries < 16) {
	    additional_entries = 16;
	}
	else if (additional_entries > 1024) {
	    additional_entries = 1024;
	}
	/* neither + nor * overflows can happen: nentry_alloc * sizeof(struct zip_entry) < UINT64_MAX */
	nalloc += additional_entries;
	realloc_size = sizeof(struct zip_entry) * (size_t)nalloc;

	if (sizeof(struct zip_entry) * (size_t)za->nentry_alloc > realloc_size) {
	    zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
	    return -1;
	}
	rentries = (zip_entry_t *)realloc(za->entry, sizeof(struct zip_entry) * (size_t)nalloc);
	if (!rentries) {
	    zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
	    return -1;
	}
	za->entry = rentries;
	za->nentry_alloc = nalloc;
    }

    idx = za->nentry++;

    _zip_entry_init(za->entry+idx);

    return (zip_int64_t)idx;
}
