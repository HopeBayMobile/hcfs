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


/*
  NOTE: Return type is signed so we can return -1 on error.
        The index can not be larger than ZIP_INT64_MAX since the size
        of the central directory cannot be larger than
        ZIP_UINT64_MAX, and each entry is larger than 2 bytes.
*/

ZIP_EXTERN zip_int64_t
zip_add(zip_t *za, const char *name, zip_source_t *source)
{
    return zip_file_add(za, name, source, 0);
}
