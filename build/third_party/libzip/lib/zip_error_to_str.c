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
#include <stdlib.h>
#include <string.h>

#define _ZIP_COMPILING_DEPRECATED
#include "zipint.h"


ZIP_EXTERN int
zip_error_to_str(char *buf, zip_uint64_t len, int ze, int se)
{
    const char *zs, *ss;

    if (ze < 0 || ze >= _zip_nerr_str)
	return snprintf(buf, len, "Unknown error %d", ze);

    zs = _zip_err_str[ze];
	
    switch (_zip_err_type[ze]) {
    case ZIP_ET_SYS:
	ss = strerror(se);
	break;
	
    case ZIP_ET_ZLIB:
	ss = zError(se);
	break;
	
    default:
	ss = NULL;
    }

    return snprintf(buf, len, "%s%s%s",
		    zs, (ss ? ": " : ""), (ss ? ss : ""));
}
