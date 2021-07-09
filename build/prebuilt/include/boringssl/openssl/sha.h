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

#ifndef OPENSSL_HEADER_SHA_H
#define OPENSSL_HEADER_SHA_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* The SHA family of hash functions (SHA-1 and SHA-2). */


/* SHA_CBLOCK is the block size of SHA-1. */
#define SHA_CBLOCK 64

/* SHA_DIGEST_LENGTH is the length of a SHA-1 digest. */
#define SHA_DIGEST_LENGTH 20

/* TODO(fork): remove */
#define SHA_LBLOCK 16
#define SHA_LONG uint32_t

/* SHA1_Init initialises |sha| and returns one. */
OPENSSL_EXPORT int SHA1_Init(SHA_CTX *sha);

/* SHA1_Update adds |len| bytes from |data| to |sha| and returns one. */
OPENSSL_EXPORT int SHA1_Update(SHA_CTX *sha, const void *data, size_t len);

/* SHA1_Final adds the final padding to |sha| and writes the resulting digest
 * to |md|, which must have at least |SHA_DIGEST_LENGTH| bytes of space. It
 * returns one. */
OPENSSL_EXPORT int SHA1_Final(uint8_t *md, SHA_CTX *sha);

/* SHA1 writes the digest of |len| bytes from |data| to |out| and returns
 * |out|. There must be at least |SHA_DIGEST_LENGTH| bytes of space in
 * |out|. */
OPENSSL_EXPORT uint8_t *SHA1(const uint8_t *data, size_t len, uint8_t *out);

/* SHA1_Transform is a low-level function that performs a single, SHA-1 block
 * transformation using the state from |sha| and 64 bytes from |block|. */
OPENSSL_EXPORT void SHA1_Transform(SHA_CTX *sha, const uint8_t *block);

struct sha_state_st {
  uint32_t h0, h1, h2, h3, h4;
  uint32_t Nl, Nh;
  uint32_t data[16];
  unsigned int num;
};


/* SHA-224. */

/* SHA224_CBLOCK is the block size of SHA-224. */
#define SHA224_CBLOCK 64

/* SHA224_DIGEST_LENGTH is the length of a SHA-224 digest. */
#define SHA224_DIGEST_LENGTH 28

/* SHA224_Init initialises |sha| and returns 1. */
OPENSSL_EXPORT int SHA224_Init(SHA256_CTX *sha);

/* SHA224_Update adds |len| bytes from |data| to |sha| and returns 1. */
OPENSSL_EXPORT int SHA224_Update(SHA256_CTX *sha, const void *data, size_t len);

/* SHA224_Final adds the final padding to |sha| and writes the resulting digest
 * to |md|, which must have at least |SHA224_DIGEST_LENGTH| bytes of space. It
 * returns one on success and zero on programmer error. */
OPENSSL_EXPORT int SHA224_Final(uint8_t *md, SHA256_CTX *sha);

/* SHA224 writes the digest of |len| bytes from |data| to |out| and returns
 * |out|. There must be at least |SHA224_DIGEST_LENGTH| bytes of space in
 * |out|. */
OPENSSL_EXPORT uint8_t *SHA224(const uint8_t *data, size_t len, uint8_t *out);


/* SHA-256. */

/* SHA256_CBLOCK is the block size of SHA-256. */
#define SHA256_CBLOCK 64

/* SHA256_DIGEST_LENGTH is the length of a SHA-256 digest. */
#define SHA256_DIGEST_LENGTH 32

/* SHA256_Init initialises |sha| and returns 1. */
OPENSSL_EXPORT int SHA256_Init(SHA256_CTX *sha);

/* SHA256_Update adds |len| bytes from |data| to |sha| and returns 1. */
OPENSSL_EXPORT int SHA256_Update(SHA256_CTX *sha, const void *data, size_t len);

/* SHA256_Final adds the final padding to |sha| and writes the resulting digest
 * to |md|, which must have at least |SHA256_DIGEST_LENGTH| bytes of space. It
 * returns one on success and zero on programmer error. */
OPENSSL_EXPORT int SHA256_Final(uint8_t *md, SHA256_CTX *sha);

/* SHA256 writes the digest of |len| bytes from |data| to |out| and returns
 * |out|. There must be at least |SHA256_DIGEST_LENGTH| bytes of space in
 * |out|. */
OPENSSL_EXPORT uint8_t *SHA256(const uint8_t *data, size_t len, uint8_t *out);

/* SHA256_Transform is a low-level function that performs a single, SHA-1 block
 * transformation using the state from |sha| and 64 bytes from |block|. */
OPENSSL_EXPORT void SHA256_Transform(SHA256_CTX *sha, const uint8_t *data);

struct sha256_state_st {
  uint32_t h[8];
  uint32_t Nl, Nh;
  uint32_t data[16];
  unsigned int num, md_len;
};


/* SHA-384. */

/* SHA384_CBLOCK is the block size of SHA-384. */
#define SHA384_CBLOCK 128

/* SHA384_DIGEST_LENGTH is the length of a SHA-384 digest. */
#define SHA384_DIGEST_LENGTH 48

/* SHA384_Init initialises |sha| and returns 1. */
OPENSSL_EXPORT int SHA384_Init(SHA512_CTX *sha);

/* SHA384_Update adds |len| bytes from |data| to |sha| and returns 1. */
OPENSSL_EXPORT int SHA384_Update(SHA512_CTX *sha, const void *data, size_t len);

/* SHA384_Final adds the final padding to |sha| and writes the resulting digest
 * to |md|, which must have at least |SHA384_DIGEST_LENGTH| bytes of space. It
 * returns one on success and zero on programmer error. */
OPENSSL_EXPORT int SHA384_Final(uint8_t *md, SHA512_CTX *sha);

/* SHA384 writes the digest of |len| bytes from |data| to |out| and returns
 * |out|. There must be at least |SHA384_DIGEST_LENGTH| bytes of space in
 * |out|. */
OPENSSL_EXPORT uint8_t *SHA384(const uint8_t *data, size_t len, uint8_t *out);

/* SHA384_Transform is a low-level function that performs a single, SHA-1 block
 * transformation using the state from |sha| and 64 bytes from |block|. */
OPENSSL_EXPORT void SHA384_Transform(SHA512_CTX *sha, const uint8_t *data);


/* SHA-512. */

/* SHA512_CBLOCK is the block size of SHA-512. */
#define SHA512_CBLOCK 128

/* SHA512_DIGEST_LENGTH is the length of a SHA-512 digest. */
#define SHA512_DIGEST_LENGTH 64

/* SHA512_Init initialises |sha| and returns 1. */
OPENSSL_EXPORT int SHA512_Init(SHA512_CTX *sha);

/* SHA512_Update adds |len| bytes from |data| to |sha| and returns 1. */
OPENSSL_EXPORT int SHA512_Update(SHA512_CTX *sha, const void *data, size_t len);

/* SHA512_Final adds the final padding to |sha| and writes the resulting digest
 * to |md|, which must have at least |SHA512_DIGEST_LENGTH| bytes of space. It
 * returns one on success and zero on programmer error. */
OPENSSL_EXPORT int SHA512_Final(uint8_t *md, SHA512_CTX *sha);

/* SHA512 writes the digest of |len| bytes from |data| to |out| and returns
 * |out|. There must be at least |SHA512_DIGEST_LENGTH| bytes of space in
 * |out|. */
OPENSSL_EXPORT uint8_t *SHA512(const uint8_t *data, size_t len, uint8_t *out);

/* SHA512_Transform is a low-level function that performs a single, SHA-1 block
 * transformation using the state from |sha| and 64 bytes from |block|. */
OPENSSL_EXPORT void SHA512_Transform(SHA512_CTX *sha, const uint8_t *data);

struct sha512_state_st {
  uint64_t h[8];
  uint64_t Nl, Nh;
  union {
    uint64_t d[16];
    uint8_t p[128];
  } u;
  unsigned int num, md_len;
};


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_SHA_H */
