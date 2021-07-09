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


/* zip_discard:
   frees the space allocated to a zipfile struct, and closes the
   corresponding file. */

void
zip_discard(zip_t *za)
{
    zip_uint64_t i;

    if (za == NULL)
	return;

    if (za->src) {
	zip_source_close(za->src);
	zip_source_free(za->src);
    }

    free(za->default_password);
    _zip_string_free(za->comment_orig);
    _zip_string_free(za->comment_changes);

    _zip_hash_free(za->names);

    if (za->entry) {
	for (i=0; i<za->nentry; i++)
	    _zip_entry_finalize(za->entry+i);
	free(za->entry);
    }

    for (i=0; i<za->nopen_source; i++) {
	_zip_source_invalidate(za->open_source[i]);
    }
    free(za->open_source);

    zip_error_fini(&za->error);
    
    free(za);

    return;
}
