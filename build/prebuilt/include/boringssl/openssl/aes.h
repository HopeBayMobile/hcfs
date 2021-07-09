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

#ifndef OPENSSL_HEADER_AES_H
#define OPENSSL_HEADER_AES_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* Raw AES functions. */


#define AES_ENCRYPT 1
#define AES_DECRYPT 0

/* AES_MAXNR is the maximum number of AES rounds. */
#define AES_MAXNR 14

#define AES_BLOCK_SIZE 16

/* aes_key_st should be an opaque type, but EVP requires that the size be
 * known. */
struct aes_key_st {
  uint32_t rd_key[4 * (AES_MAXNR + 1)];
  unsigned rounds;
};
typedef struct aes_key_st AES_KEY;

/* AES_set_encrypt_key configures |aeskey| to encrypt with the |bits|-bit key,
 * |key|.
 *
 * WARNING: unlike other OpenSSL functions, this returns zero on success and a
 * negative number on error. */
OPENSSL_EXPORT int AES_set_encrypt_key(const uint8_t *key, unsigned bits,
                                       AES_KEY *aeskey);

/* AES_set_decrypt_key configures |aeskey| to decrypt with the |bits|-bit key,
 * |key|.
 *
 * WARNING: unlike other OpenSSL functions, this returns zero on success and a
 * negative number on error. */
OPENSSL_EXPORT int AES_set_decrypt_key(const uint8_t *key, unsigned bits,
                                       AES_KEY *aeskey);

/* AES_encrypt encrypts a single block from |in| to |out| with |key|. The |in|
 * and |out| pointers may overlap. */
OPENSSL_EXPORT void AES_encrypt(const uint8_t *in, uint8_t *out,
                                const AES_KEY *key);

/* AES_decrypt decrypts a single block from |in| to |out| with |key|. The |in|
 * and |out| pointers may overlap. */
OPENSSL_EXPORT void AES_decrypt(const uint8_t *in, uint8_t *out,
                                const AES_KEY *key);


/* Block cipher modes. */

/* AES_ctr128_encrypt encrypts (or decrypts, it's the same in CTR mode) |len|
 * bytes from |in| to |out|. The |num| parameter must be set to zero on the
 * first call and |ivec| will be incremented. */
OPENSSL_EXPORT void AES_ctr128_encrypt(const uint8_t *in, uint8_t *out,
                                       size_t len, const AES_KEY *key,
                                       uint8_t ivec[AES_BLOCK_SIZE],
                                       uint8_t ecount_buf[AES_BLOCK_SIZE],
                                       unsigned int *num);

/* AES_ecb_encrypt encrypts (or decrypts, if |enc| == |AES_DECRYPT|) a single,
 * 16 byte block from |in| to |out|. */
OPENSSL_EXPORT void AES_ecb_encrypt(const uint8_t *in, uint8_t *out,
                                    const AES_KEY *key, const int enc);

/* AES_cbc_encrypt encrypts (or decrypts, if |enc| == |AES_DECRYPT|) |len|
 * bytes from |in| to |out|. The length must be a multiple of the block size. */
OPENSSL_EXPORT void AES_cbc_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                                    const AES_KEY *key, uint8_t *ivec,
                                    const int enc);

/* AES_ofb128_encrypt encrypts (or decrypts, it's the same in CTR mode) |len|
 * bytes from |in| to |out|. The |num| parameter must be set to zero on the
 * first call. */
OPENSSL_EXPORT void AES_ofb128_encrypt(const uint8_t *in, uint8_t *out,
                                       size_t len, const AES_KEY *key,
                                       uint8_t *ivec, int *num);

/* AES_cfb128_encrypt encrypts (or decrypts, if |enc| == |AES_DECRYPT|) |len|
 * bytes from |in| to |out|. The |num| parameter must be set to zero on the
 * first call. */
OPENSSL_EXPORT void AES_cfb128_encrypt(const uint8_t *in, uint8_t *out,
                                       size_t len, const AES_KEY *key,
                                       uint8_t *ivec, int *num, int enc);


/* Android compatibility section.
 *
 * These functions are declared, temporarily, for Android because
 * wpa_supplicant will take a little time to sync with upstream. Outside of
 * Android they'll have no definition. */

OPENSSL_EXPORT int AES_wrap_key(AES_KEY *key, const uint8_t *iv, uint8_t *out,
                                const uint8_t *in, unsigned in_len);
OPENSSL_EXPORT int AES_unwrap_key(AES_KEY *key, const uint8_t *iv, uint8_t *out,
                                  const uint8_t *in, unsigned in_len);


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_AES_H */
