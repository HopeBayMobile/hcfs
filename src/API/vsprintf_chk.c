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
#include <stdarg.h>

/*
 * Runtime implementation of __builtin____vsprintf_chk.
 *
 * See
 *   http://gcc.gnu.org/onlinedocs/gcc/Object-Size-Checking.html
 *   http://gcc.gnu.org/ml/gcc-patches/2004-09/msg02055.html
 * for details.
 *
 * This vsprintf check is called if _FORTIFY_SOURCE is defined and
 * greater than 0.
 */
int __vsprintf_chk(
        char *dest,
        int flags,
        size_t dest_len_from_compiler,
        const char *format,
        va_list va)
{
    int ret = vsnprintf(dest, dest_len_from_compiler, format, va);

    return ret;
}

/*
 * Runtime implementation of __builtin____sprintf_chk.
 *
 * See
 *   http://gcc.gnu.org/onlinedocs/gcc/Object-Size-Checking.html
 *   http://gcc.gnu.org/ml/gcc-patches/2004-09/msg02055.html
 * for details.
 *
 * This sprintf check is called if _FORTIFY_SOURCE is defined and
 * greater than 0.
 */
int __sprintf_chk(
        char *dest,
        int flags,
        size_t dest_len_from_compiler,
        const char *format, ...)
{
    va_list va;
    int retval;

    va_start(va, format);
    retval = __vsprintf_chk(dest, flags,
                             dest_len_from_compiler, format, va);
    va_end(va);

    return retval;
}
