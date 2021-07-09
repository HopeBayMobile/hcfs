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
zip_source_layered(zip_t *za, zip_source_t *src, zip_source_layered_callback cb, void *ud)
{
    if (za == NULL)
        return NULL;

    return zip_source_layered_create(src, cb, ud, &za->error);
}


zip_source_t *
zip_source_layered_create(zip_source_t *src, zip_source_layered_callback cb, void *ud, zip_error_t *error)
{
    zip_source_t *zs;
    
    if ((zs=_zip_source_new(error)) == NULL)
        return NULL;
    
    zip_source_keep(src);
    zs->src = src;
    zs->cb.l = cb;
    zs->ud = ud;

    zs->supports = cb(src, ud, NULL, 0, ZIP_SOURCE_SUPPORTS);
    if (zs->supports < 0) {
        zs->supports = ZIP_SOURCE_SUPPORTS_READABLE;
    }

    return zs;
}
