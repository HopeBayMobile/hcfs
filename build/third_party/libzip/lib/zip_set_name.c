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


int
_zip_set_name(zip_t *za, zip_uint64_t idx, const char *name, zip_flags_t flags)
{
    zip_entry_t *e;
    zip_string_t *str;
    bool same_as_orig;
    zip_int64_t i;
    const zip_uint8_t *old_name, *new_name;
    zip_string_t *old_str;

    if (idx >= za->nentry) {
	zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return -1;
    }

    if (ZIP_IS_RDONLY(za)) {
	zip_error_set(&za->error, ZIP_ER_RDONLY, 0);
	return -1;
    }

    if (name && name[0] != '\0') {
        /* TODO: check for string too long */
	if ((str=_zip_string_new((const zip_uint8_t *)name, (zip_uint16_t)strlen(name), flags, &za->error)) == NULL)
	    return -1;
	if ((flags & ZIP_FL_ENCODING_ALL) == ZIP_FL_ENC_GUESS && _zip_guess_encoding(str, ZIP_ENCODING_UNKNOWN) == ZIP_ENCODING_UTF8_GUESSED)
	    str->encoding = ZIP_ENCODING_UTF8_KNOWN;
    }
    else
	str = NULL;

    /* TODO: encoding flags needed for CP437? */
    if ((i=_zip_name_locate(za, name, 0, NULL)) >= 0 && (zip_uint64_t)i != idx) {
	_zip_string_free(str);
	zip_error_set(&za->error, ZIP_ER_EXISTS, 0);
	return -1;
    }

    /* no effective name change */
    if (i>=0 && (zip_uint64_t)i == idx) {
	_zip_string_free(str);
	return 0;
    }

    e = za->entry+idx;

    if (e->orig)
	same_as_orig = _zip_string_equal(e->orig->filename, str);
    else
	same_as_orig = false;

    if (!same_as_orig && e->changes == NULL) {
	if ((e->changes=_zip_dirent_clone(e->orig)) == NULL) {
	    zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
	    _zip_string_free(str);
	    return -1;
	}
    }

    if ((new_name = _zip_string_get(same_as_orig ? e->orig->filename : str, NULL, 0, &za->error)) == NULL) {
	_zip_string_free(str);
	return -1;
    }

    if (e->changes) {
	old_str = e->changes->filename;
    }
    else if (e->orig) {
	old_str = e->orig->filename;
    }
    else {
	old_str = NULL;
    }
    
    if (old_str) {
	if ((old_name = _zip_string_get(old_str, NULL, 0, &za->error)) == NULL) {
	    _zip_string_free(str);
	    return -1;
	}
    }
    else {
	old_name = NULL;
    }

    if (_zip_hash_add(za->names, new_name, idx, 0, &za->error) == false) {
	_zip_string_free(str);
	return -1;
    }
    if (old_name) {
	_zip_hash_delete(za->names, old_name, NULL);
    }

    if (same_as_orig) {
	if (e->changes) {
	    if (e->changes->changed & ZIP_DIRENT_FILENAME) {
		_zip_string_free(e->changes->filename);
		e->changes->changed &= ~ZIP_DIRENT_FILENAME;
		if (e->changes->changed == 0) {
		    _zip_dirent_free(e->changes);
		    e->changes = NULL;
		}
		else {
		    /* TODO: what if not cloned? can that happen? */
		    e->changes->filename = e->orig->filename;
		}
	    }
	}
	_zip_string_free(str);
    }
    else {
	if (e->changes->changed & ZIP_DIRENT_FILENAME) {
	    _zip_string_free(e->changes->filename);
	}
	e->changes->changed |= ZIP_DIRENT_FILENAME;
	e->changes->filename = str;
    }

    return 0;
}
