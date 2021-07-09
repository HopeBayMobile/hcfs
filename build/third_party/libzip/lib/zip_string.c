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


zip_uint32_t
_zip_string_crc32(const zip_string_t *s)
{
    zip_uint32_t crc;
    
    crc = (zip_uint32_t)crc32(0L, Z_NULL, 0);

    if (s != NULL)    
	crc = (zip_uint32_t)crc32(crc, s->raw, s->length);

    return crc;
}


int
_zip_string_equal(const zip_string_t *a, const zip_string_t *b)
{
    if (a == NULL || b == NULL)
	return a == b;

    if (a->length != b->length)
	return 0;

    /* TODO: encoding */

    return (memcmp(a->raw, b->raw, a->length) == 0);
}


void
_zip_string_free(zip_string_t *s)
{
    if (s == NULL)
	return;

    free(s->raw);
    free(s->converted);
    free(s);
}


const zip_uint8_t *
_zip_string_get(zip_string_t *string, zip_uint32_t *lenp, zip_flags_t flags, zip_error_t *error)
{
    static const zip_uint8_t empty[1] = "";

    if (string == NULL) {
	if (lenp)
	    *lenp = 0;
	return empty;
    }

    if ((flags & ZIP_FL_ENC_RAW) == 0) {
	/* start guessing */
	if (string->encoding == ZIP_ENCODING_UNKNOWN)
	    _zip_guess_encoding(string, ZIP_ENCODING_UNKNOWN);

	if (((flags & ZIP_FL_ENC_STRICT)
	     && string->encoding != ZIP_ENCODING_ASCII && string->encoding != ZIP_ENCODING_UTF8_KNOWN)
	    || (string->encoding == ZIP_ENCODING_CP437)) {
	    if (string->converted == NULL) {
		if ((string->converted=_zip_cp437_to_utf8(string->raw, string->length,
							  &string->converted_length, error)) == NULL)
		    return NULL;
	    }
	    if (lenp)
		*lenp = string->converted_length;
	    return string->converted;
	}
    }
    
    if (lenp)
	*lenp = string->length;
    return string->raw;
}


zip_uint16_t
_zip_string_length(const zip_string_t *s)
{
    if (s == NULL)
	return 0;

    return s->length;
}


zip_string_t *
_zip_string_new(const zip_uint8_t *raw, zip_uint16_t length, zip_flags_t flags, zip_error_t *error)
{
    zip_string_t *s;
    zip_encoding_type_t expected_encoding;
    
    if (length == 0)
	return NULL;

    switch (flags & ZIP_FL_ENCODING_ALL) {
    case ZIP_FL_ENC_GUESS:
	expected_encoding = ZIP_ENCODING_UNKNOWN;
	break;
    case ZIP_FL_ENC_UTF_8:
	expected_encoding = ZIP_ENCODING_UTF8_KNOWN;
	break;
    case ZIP_FL_ENC_CP437:
	expected_encoding = ZIP_ENCODING_CP437;
	break;
    default:
	zip_error_set(error, ZIP_ER_INVAL, 0);
	return NULL;
    }
	
    if ((s=(zip_string_t *)malloc(sizeof(*s))) == NULL) {
	zip_error_set(error, ZIP_ER_MEMORY, 0);
	return NULL;
    }

    if ((s->raw=(zip_uint8_t *)malloc((size_t)(length+1))) == NULL) {
	free(s);
	return NULL;
    }

    memcpy(s->raw, raw, length);
    s->raw[length] = '\0';
    s->length = length;
    s->encoding = ZIP_ENCODING_UNKNOWN;
    s->converted = NULL;
    s->converted_length = 0;

    if (expected_encoding != ZIP_ENCODING_UNKNOWN) {
	if (_zip_guess_encoding(s, expected_encoding) == ZIP_ENCODING_ERROR) {
	    _zip_string_free(s);
	    zip_error_set(error, ZIP_ER_INVAL, 0);
	    return NULL;
	}
    }
    
    return s;
}


int
_zip_string_write(zip_t *za, const zip_string_t *s)
{
    if (s == NULL)
	return 0;
    
    return _zip_write(za, s->raw, s->length);
}
