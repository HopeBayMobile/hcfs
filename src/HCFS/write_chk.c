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

#include <unistd.h>

#include "macro.h"

ssize_t __write_chk(int fd, const void* buf, size_t count, size_t buf_size) {
  UNUSED(buf_size);
/*
  if (__predict_false(count > buf_size)) {
    __fortify_chk_fail("write: prevented read past end of buffer", 0);
  }

  if (__predict_false(count > SSIZE_MAX)) {
    __fortify_chk_fail("write: count > SSIZE_MAX", 0);
  }
*/

  return write(fd, buf, count);
}
