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
zip_error_code_system(const zip_error_t *error) {
    return error->sys_err;
}


ZIP_EXTERN int
zip_error_code_zip(const zip_error_t *error) {
    return error->zip_err;
}


ZIP_EXTERN void
zip_error_fini(zip_error_t *err)
{
    free(err->str);
    err->str = NULL;
}


ZIP_EXTERN void
zip_error_init(zip_error_t *err)
{
    err->zip_err = ZIP_ER_OK;
    err->sys_err = 0;
    err->str = NULL;
}

ZIP_EXTERN void
zip_error_init_with_code(zip_error_t *error, int ze)
{
    zip_error_init(error);
    error->zip_err = ze;
    switch (zip_error_system_type(error)) {
    case ZIP_ET_SYS:
	error->sys_err = errno;
	break;

    default:
	error->sys_err = 0;
	break;
    }	
}


ZIP_EXTERN int
zip_error_system_type(const zip_error_t *error) {
    if (error->zip_err < 0 || error->zip_err >= _zip_nerr_str)
        return ZIP_ET_NONE;
    
    return _zip_err_type[error->zip_err];
}


void
_zip_error_clear(zip_error_t *err)
{
    if (err == NULL)
	return;

    err->zip_err = ZIP_ER_OK;
    err->sys_err = 0;
}


void
_zip_error_copy(zip_error_t *dst, const zip_error_t *src)
{
    dst->zip_err = src->zip_err;
    dst->sys_err = src->sys_err;
}


void
_zip_error_get(const zip_error_t *err, int *zep, int *sep)
{
    if (zep)
	*zep = err->zip_err;
    if (sep) {
	if (zip_error_system_type(err) != ZIP_ET_NONE)
	    *sep = err->sys_err;
	else
	    *sep = 0;
    }
}


void
zip_error_set(zip_error_t *err, int ze, int se)
{
    if (err) {
	err->zip_err = ze;
	err->sys_err = se;
    }
}


void
_zip_error_set_from_source(zip_error_t *err, zip_source_t *src)
{
    _zip_error_copy(err, zip_source_error(src));
}


zip_int64_t
zip_error_to_data(const zip_error_t *error, void *data, zip_uint64_t length)
{
    int *e = (int *)data;
    
    if (length < sizeof(int)*2) {
        return -1;
    }
    
    e[0] = zip_error_code_zip(error);
    e[1] = zip_error_code_system(error);
    return sizeof(int)*2;
}
