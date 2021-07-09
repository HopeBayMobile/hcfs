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


#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "zipint.h"


ZIP_EXTERN zip_int64_t
zip_name_locate(zip_t *za, const char *fname, zip_flags_t flags)
{
    return _zip_name_locate(za, fname, flags, &za->error);
}


zip_int64_t
_zip_name_locate(zip_t *za, const char *fname, zip_flags_t flags, zip_error_t *error)
{
    int (*cmp)(const char *, const char *);
    const char *fn, *p;
    zip_uint64_t i;

    if (za == NULL)
	return -1;

    if (fname == NULL) {
	zip_error_set(error, ZIP_ER_INVAL, 0);
	return -1;
    }

    if (flags & (ZIP_FL_NOCASE|ZIP_FL_NODIR|ZIP_FL_ENC_CP437)) {
	/* can't use hash table */
	cmp = (flags & ZIP_FL_NOCASE) ? strcasecmp : strcmp;

	for (i=0; i<za->nentry; i++) {
	    fn = _zip_get_name(za, i, flags, error);
	    
	    /* newly added (partially filled) entry or error */
	    if (fn == NULL)
		continue;
	    
	    if (flags & ZIP_FL_NODIR) {
		p = strrchr(fn, '/');
		if (p)
		    fn = p+1;
	    }
	    
	    if (cmp(fname, fn) == 0) {
		_zip_error_clear(error);
		return (zip_int64_t)i;
	    }
	}

	zip_error_set(error, ZIP_ER_NOENT, 0);
	return -1;
    }
    else {
	return _zip_hash_lookup(za->names, (const zip_uint8_t *)fname, flags, error);
    }
}
