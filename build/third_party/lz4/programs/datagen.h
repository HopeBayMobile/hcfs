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


#include <stddef.h>   /* size_t */

void RDG_genOut(unsigned long long size, double matchProba, double litProba, unsigned seed);
void RDG_genBuffer(void* buffer, size_t size, double matchProba, double litProba, unsigned seed);
/* RDG_genOut
   Generate 'size' bytes of compressible data into stdout.
   Compressibility can be controlled using 'matchProba'.
   'LitProba' is optional, and affect variability of bytes. If litProba==0.0, default value is used.
   Generated data can be selected using 'seed'.
   If (matchProba, litProba and seed) are equal, the function always generate the same content.

   RDG_genBuffer
   Same as RDG_genOut, but generate data into provided buffer
*/
