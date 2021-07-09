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
zip_set_archive_comment(zip_t *za, const char *comment, zip_uint16_t len)
{
    zip_string_t *cstr;

    if (ZIP_IS_RDONLY(za)) {
	zip_error_set(&za->error, ZIP_ER_RDONLY, 0);
	return -1;
    }

    if (len > 0 && comment == NULL) {
	zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return -1;
    }

    if (len > 0) {
	if ((cstr=_zip_string_new((const zip_uint8_t *)comment, len, ZIP_FL_ENC_GUESS, &za->error)) == NULL)
	    return -1;

	if (_zip_guess_encoding(cstr, ZIP_ENCODING_UNKNOWN) == ZIP_ENCODING_CP437) {
	    _zip_string_free(cstr);
	    zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	    return -1;
	}
    }
    else
	cstr = NULL;

    _zip_string_free(za->comment_changes);
    za->comment_changes = NULL;

    if (((za->comment_orig && _zip_string_equal(za->comment_orig, cstr))
	 || (za->comment_orig == NULL && cstr == NULL))) {
	_zip_string_free(cstr);
	za->comment_changed = 0;
    }
    else {
	za->comment_changes = cstr;
	za->comment_changed = 1;
    }
    
    return 0;
}
