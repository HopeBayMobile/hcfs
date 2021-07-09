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

#include "macro.h"

size_t __fread_chk(void * buf, size_t size, size_t count,
                              FILE * stream, size_t buf_size) {
  UNUSED(buf_size);
/*
  size_t total;
  if (__predict_false(__size_mul_overflow(size, count, &total))) {
    // overflow: trigger the error path in fread
    return fread(buf, size, count, stream);
  }

  if (__predict_false(total > buf_size)) {
    __fortify_chk_fail("fread: prevented write past end of buffer", 0);
  }
*/

  return fread(buf, size, count, stream);
}
