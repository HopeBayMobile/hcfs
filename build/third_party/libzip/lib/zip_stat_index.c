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


ZIP_EXTERN int
zip_stat_index(zip_t *za, zip_uint64_t index, zip_flags_t flags,
	       zip_stat_t *st)
{
    const char *name;
    zip_dirent_t *de;

    if ((de=_zip_get_dirent(za, index, flags, NULL)) == NULL)
	return -1;

    if ((name=zip_get_name(za, index, flags)) == NULL)
	return -1;
    

    if ((flags & ZIP_FL_UNCHANGED) == 0
	&& ZIP_ENTRY_DATA_CHANGED(za->entry+index)) {
	if (zip_source_stat(za->entry[index].source, st) < 0) {
	    zip_error_set(&za->error, ZIP_ER_CHANGED, 0);
	    return -1;
	}
    }
    else {
	zip_stat_init(st);

	st->crc = de->crc;
	st->size = de->uncomp_size;
	st->mtime = de->last_mod;
	st->comp_size = de->comp_size;
	st->comp_method = (zip_uint16_t)de->comp_method;
	if (de->bitflags & ZIP_GPBF_ENCRYPTED) {
	    if (de->bitflags & ZIP_GPBF_STRONG_ENCRYPTION) {
		/* TODO */
		st->encryption_method = ZIP_EM_UNKNOWN;
	    }
	    else
		st->encryption_method = ZIP_EM_TRAD_PKWARE;
	}
	else
	    st->encryption_method = ZIP_EM_NONE;
	st->valid = ZIP_STAT_CRC|ZIP_STAT_SIZE|ZIP_STAT_MTIME
	    |ZIP_STAT_COMP_SIZE|ZIP_STAT_COMP_METHOD|ZIP_STAT_ENCRYPTION_METHOD;
    }

    st->index = index;
    st->name = name;
    st->valid |= ZIP_STAT_INDEX|ZIP_STAT_NAME;
    
    return 0;
}
