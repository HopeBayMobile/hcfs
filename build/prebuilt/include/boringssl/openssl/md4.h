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

#ifndef OPENSSL_HEADER_MD4_H
#define OPENSSL_HEADER_MD4_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* MD4. */

/* MD4_CBLOCK is the block size of MD4. */
#define MD4_CBLOCK 64

/* MD4_DIGEST_LENGTH is the length of an MD4 digest. */
#define MD4_DIGEST_LENGTH 16

/* MD41_Init initialises |md4| and returns one. */
OPENSSL_EXPORT int MD4_Init(MD4_CTX *md4);

/* MD4_Update adds |len| bytes from |data| to |md4| and returns one. */
OPENSSL_EXPORT int MD4_Update(MD4_CTX *md4, const void *data, size_t len);

/* MD4_Final adds the final padding to |md4| and writes the resulting digest to
 * |md|, which must have at least |MD4_DIGEST_LENGTH| bytes of space. It
 * returns one. */
OPENSSL_EXPORT int MD4_Final(uint8_t *md, MD4_CTX *md4);

/* MD4_Transform is a low-level function that performs a single, MD4 block
 * transformation using the state from |md4| and 64 bytes from |block|. */
OPENSSL_EXPORT void MD4_Transform(MD4_CTX *md4, const uint8_t *block);

struct md4_state_st {
  uint32_t A, B, C, D;
  uint32_t Nl, Nh;
  uint32_t data[16];
  unsigned int num;
};


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_MD4_H */
