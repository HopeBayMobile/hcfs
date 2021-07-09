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


ZIP_EXTERN zip_int64_t
zip_fread(zip_file_t *zf, void *outbuf, zip_uint64_t toread)
{
    zip_int64_t n;

    if (!zf)
	return -1;

    if (zf->error.zip_err != 0)
	return -1;

    if (toread > ZIP_INT64_MAX) {
	zip_error_set(&zf->error, ZIP_ER_INVAL, 0);
	return -1;
    }

    if ((zf->eof) || (toread == 0))
	return 0;

    if ((n=zip_source_read(zf->src, outbuf, toread)) < 0) {
	_zip_error_set_from_source(&zf->error, zf->src);
	return -1;
    }

    return n;
}
