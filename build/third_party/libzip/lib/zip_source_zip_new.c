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


zip_source_t *
_zip_source_zip_new(zip_t *za, zip_t *srcza, zip_uint64_t srcidx, zip_flags_t flags, zip_uint64_t start, zip_uint64_t len, const char *password)
{
    zip_compression_implementation comp_impl;
    zip_encryption_implementation enc_impl;
    zip_source_t *src, *s2;
    zip_uint64_t offset;
    struct zip_stat st;

    if (za == NULL)
	return NULL;

    if (srcza == NULL || srcidx >= srcza->nentry) {
	zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return NULL;
    }

    if ((flags & ZIP_FL_UNCHANGED) == 0
	&& (ZIP_ENTRY_DATA_CHANGED(srcza->entry+srcidx) || srcza->entry[srcidx].deleted)) {
	zip_error_set(&za->error, ZIP_ER_CHANGED, 0);
	return NULL;
    }

    if (zip_stat_index(srcza, srcidx, flags|ZIP_FL_UNCHANGED, &st) < 0) {
	zip_error_set(&za->error, ZIP_ER_INTERNAL, 0);
	return NULL;
    }

    if (flags & ZIP_FL_ENCRYPTED)
	flags |= ZIP_FL_COMPRESSED;

    if ((start > 0 || len > 0) && (flags & ZIP_FL_COMPRESSED)) {
	zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return NULL;
    }

    /* overflow or past end of file */
    if ((start > 0 || len > 0) && (start+len < start || start+len > st.size)) {
	zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return NULL;
    }

    enc_impl = NULL;
    if (((flags & ZIP_FL_ENCRYPTED) == 0) && (st.encryption_method != ZIP_EM_NONE)) {
	if (password == NULL) {
	    zip_error_set(&za->error, ZIP_ER_NOPASSWD, 0);
	    return NULL;
	}
	if ((enc_impl=_zip_get_encryption_implementation(st.encryption_method)) == NULL) {
	    zip_error_set(&za->error, ZIP_ER_ENCRNOTSUPP, 0);
	    return NULL;
	}
    }

    comp_impl = NULL;
    if ((flags & ZIP_FL_COMPRESSED) == 0) {
	if (st.comp_method != ZIP_CM_STORE) {
	    if ((comp_impl=_zip_get_compression_implementation(st.comp_method)) == NULL) {
		zip_error_set(&za->error, ZIP_ER_COMPNOTSUPP, 0);
		return NULL;
	    }
	}
    }

    if ((offset=_zip_file_get_offset(srcza, srcidx, &za->error)) == 0)
	return NULL;

    if (st.comp_size == 0) {
	return zip_source_buffer(za, NULL, 0, 0);
    }

    if (start+len > 0 && enc_impl == NULL && comp_impl == NULL) {
	struct zip_stat st2;
	
	st2.size = len ? len : st.size-start;
	st2.comp_size = st2.size;
	st2.comp_method = ZIP_CM_STORE;
	st2.mtime = st.mtime;
	st2.valid = ZIP_STAT_SIZE|ZIP_STAT_COMP_SIZE|ZIP_STAT_COMP_METHOD|ZIP_STAT_MTIME;
	
	if ((src = _zip_source_window_new(srcza->src, offset+start, st2.size, &st2, &za->error)) == NULL) {
	    return NULL;
	}
    }
    else {
	if ((src = _zip_source_window_new(srcza->src, offset, st.comp_size, &st, &za->error)) == NULL) {
	    return NULL;
	}
    }
	
    if (_zip_source_set_source_archive(src, srcza) < 0) {
	zip_source_free(src);
	return NULL;
    }

    /* creating a layered source calls zip_keep() on the lower layer, so we free it */
	
    if (enc_impl) {
	s2 = enc_impl(za, src, st.encryption_method, 0, password);
	zip_source_free(src);
	if (s2 == NULL) {
	    return NULL;
	}
	src = s2;
    }
    if (comp_impl) {
	s2 = comp_impl(za, src, st.comp_method, 0);
	zip_source_free(src);
	if (s2 == NULL) {
	    return NULL;
	}
	src = s2;
    }
    if (((flags & ZIP_FL_COMPRESSED) == 0 || st.comp_method == ZIP_CM_STORE) && (len == 0 || len == st.comp_size)) {
	/* when reading the whole file, check for CRC errors */
	s2 = zip_source_crc(za, src, 1);
	zip_source_free(src);
	if (s2 == NULL) {
	    return NULL;
	}
	src = s2;
    }

    if (start+len > 0 && (comp_impl || enc_impl)) {
	s2 = zip_source_window(za, src, start, len ? len : st.size-start);
	zip_source_free(src);
	if (s2 == NULL) {
	    return NULL;
	}
	src = s2;
    }

    return src;
}
