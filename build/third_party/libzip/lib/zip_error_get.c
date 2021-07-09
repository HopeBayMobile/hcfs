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


#define _ZIP_COMPILING_DEPRECATED
#include "zipint.h"


ZIP_EXTERN void
zip_error_get(zip_t *za, int *zep, int *sep)
{
    _zip_error_get(&za->error, zep, sep);
}


ZIP_EXTERN zip_error_t *
zip_get_error(zip_t *za)
{
    return &za->error;
}


ZIP_EXTERN zip_error_t *
zip_file_get_error(zip_file_t *f)
{
    return &f->error;
}
