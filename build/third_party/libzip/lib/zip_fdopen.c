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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


ZIP_EXTERN zip_t *
zip_fdopen(int fd_orig, int _flags, int *zep)
{
    int fd;
    FILE *fp;
    zip_t *za;
    zip_source_t *src;
    struct zip_error error;

    if (_flags < 0 || (_flags & ZIP_TRUNCATE)) {
	_zip_set_open_error(zep, NULL, ZIP_ER_INVAL);
        return  NULL;
    }
        
    /* We dup() here to avoid messing with the passed in fd.
       We could not restore it to the original state in case of error. */

    if ((fd=dup(fd_orig)) < 0) {
	_zip_set_open_error(zep, NULL, ZIP_ER_OPEN);
	return NULL;
    }

    if ((fp=fdopen(fd, "rb")) == NULL) {
	close(fd);
	_zip_set_open_error(zep, NULL, ZIP_ER_OPEN);
	return NULL;
    }

    zip_error_init(&error);
    if ((src = zip_source_filep_create(fp, 0, -1, &error)) == NULL) {
	_zip_set_open_error(zep, &error, 0);
	zip_error_fini(&error);
	return NULL;
    }

    if ((za = zip_open_from_source(src, _flags, &error)) == NULL) {
	_zip_set_open_error(zep, &error, 0);
	zip_error_fini(&error);
	return NULL;
    }

    zip_error_fini(&error);
    close(fd_orig);
    return za;
}
