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

/* crypto/ebcdic.h */

#ifndef HEADER_EBCDIC_H
# define HEADER_EBCDIC_H

# include <sys/types.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* Avoid name clashes with other applications */
# define os_toascii   _openssl_os_toascii
# define os_toebcdic  _openssl_os_toebcdic
# define ebcdic2ascii _openssl_ebcdic2ascii
# define ascii2ebcdic _openssl_ascii2ebcdic

extern const unsigned char os_toascii[256];
extern const unsigned char os_toebcdic[256];
void *ebcdic2ascii(void *dest, const void *srce, size_t count);
void *ascii2ebcdic(void *dest, const void *srce, size_t count);

#ifdef  __cplusplus
}
#endif
#endif
