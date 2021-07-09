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


#include <stdio.h>

#include "zipint.h"



int
_zip_filerange_crc(zip_source_t *src, zip_uint64_t start, zip_uint64_t len, uLong *crcp, zip_error_t *error)
{
    Bytef buf[BUFSIZE];
    zip_int64_t n;

    *crcp = crc32(0L, Z_NULL, 0);

    if (start > ZIP_INT64_MAX) {
	zip_error_set(error, ZIP_ER_SEEK, EFBIG);
	return -1;
    }

    if (zip_source_seek(src, (zip_int64_t)start, SEEK_SET) != 0) {
	_zip_error_set_from_source(error, src);
	return -1;
    }
    
    while (len > 0) {
	n = (zip_int64_t)(len > BUFSIZE ? BUFSIZE : len);
	if ((n = zip_source_read(src, buf, (zip_uint64_t)n)) < 0) {
	    _zip_error_set_from_source(error, src);
	    return -1;
	}
	if (n == 0) {
	    zip_error_set(error, ZIP_ER_EOF, 0);
	    return -1;
	}

	*crcp = crc32(*crcp, buf, (uInt)n);

	len -= (zip_uint64_t)n;
    }

    return 0;
}
