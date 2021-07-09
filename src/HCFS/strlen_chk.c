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

#include "macro.h"

/*
 * Runtime implementation of __strlen_chk.
 *
 * See
 *   http://gcc.gnu.org/onlinedocs/gcc/Object-Size-Checking.html
 *   http://gcc.gnu.org/ml/gcc-patches/2004-09/msg02055.html
 * for details.
 *
 * This strlen check is called if _FORTIFY_SOURCE is defined and
 * greater than 0.
 *
 * This test is designed to detect code such as:
 *
 * int main() {
 *   char buf[10];
 *   memcpy(buf, "1234567890", sizeof(buf));
 *   size_t len = strlen(buf); // segfault here with _FORTIFY_SOURCE
 *   printf("%d\n", len);
 *   return 0;
 * }
 *
 * or anytime strlen reads beyond an object boundary.
 */
size_t __strlen_chk(const char *s, size_t s_len) {
    UNUSED(s_len);
    size_t ret = strlen(s);

    return ret;
}
