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


ZIP_EXTERN int
zip_file_set_comment(zip_t *za, zip_uint64_t idx,
		     const char *comment, zip_uint16_t len, zip_flags_t flags)
{
    zip_entry_t *e;
    zip_string_t *cstr;
    int changed;

    if (_zip_get_dirent(za, idx, 0, NULL) == NULL)
	return -1;

    if (ZIP_IS_RDONLY(za)) {
	zip_error_set(&za->error, ZIP_ER_RDONLY, 0);
	return -1;
    }

    if (len > 0 && comment == NULL) {
	zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return -1;
    }

    if (len > 0) {
	if ((cstr=_zip_string_new((const zip_uint8_t *)comment, len, flags, &za->error)) == NULL)
	    return -1;
	if ((flags & ZIP_FL_ENCODING_ALL) == ZIP_FL_ENC_GUESS && _zip_guess_encoding(cstr, ZIP_ENCODING_UNKNOWN) == ZIP_ENCODING_UTF8_GUESSED)
	    cstr->encoding = ZIP_ENCODING_UTF8_KNOWN;
    }
    else
	cstr = NULL;

    e = za->entry+idx;

    if (e->changes) {
	_zip_string_free(e->changes->comment);
	e->changes->comment = NULL;
	e->changes->changed &= ~ZIP_DIRENT_COMMENT;
    }

    if (e->orig && e->orig->comment)
	changed = !_zip_string_equal(e->orig->comment, cstr);
    else
	changed = (cstr != NULL);
	
    if (changed) {
        if (e->changes == NULL) {
            if ((e->changes=_zip_dirent_clone(e->orig)) == NULL) {
                zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
		_zip_string_free(cstr);
                return -1;
            }
        }
        e->changes->comment = cstr;
        e->changes->changed |= ZIP_DIRENT_COMMENT;
    }
    else {
	_zip_string_free(cstr);
	if (e->changes && e->changes->changed == 0) {
	    _zip_dirent_free(e->changes);
	    e->changes = NULL;
	}
    }

    return 0;
}
