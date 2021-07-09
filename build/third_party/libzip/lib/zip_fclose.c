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
zip_fclose(zip_file_t *zf)
{
    int ret;
    
    if (zf->src)
	zip_source_free(zf->src);

    ret = 0;
    if (zf->error.zip_err)
	ret = zf->error.zip_err;

    zip_error_fini(&zf->error);
    free(zf);
    return ret;
}
