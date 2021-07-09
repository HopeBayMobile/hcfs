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
#include <stdlib.h>

/*
 * Runtime implementation of __builtin____strcat_chk.
 *
 * See
 *   http://gcc.gnu.org/onlinedocs/gcc/Object-Size-Checking.html
 *   http://gcc.gnu.org/ml/gcc-patches/2004-09/msg02055.html
 * for details.
 *
 * This strcat check is called if _FORTIFY_SOURCE is defined and
 * greater than 0.
 */
char* __strcat_chk(
        char* __restrict dest,
        const char* __restrict src,
        size_t dest_buf_size)
{
    char* save = dest;
    size_t dest_len = strlen(dest);

    dest += dest_len;
    dest_buf_size -= dest_len;

    while ((*dest++ = *src++) != '\0') {
        dest_buf_size--;
    }

    return save;
}

