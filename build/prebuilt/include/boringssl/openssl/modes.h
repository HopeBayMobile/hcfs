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

#ifndef OPENSSL_HEADER_MODES_H
#define OPENSSL_HEADER_MODES_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* modes.h contains functions that implement various block-cipher modes. */


/* block128_f is the type of a 128-bit, block cipher. */
typedef void (*block128_f)(const uint8_t in[16], uint8_t out[16],
                           const void *key);


/* CTR. */

/* ctr128_f is the type of a function that performs CTR-mode encryption. */
typedef void (*ctr128_f)(const uint8_t *in, uint8_t *out, size_t blocks,
                         const void *key, const uint8_t ivec[16]);

/* CRYPTO_ctr128_encrypt encrypts (or decrypts, it's the same in CTR mode)
 * |len| bytes from |in| to |out| using |block| in counter mode. There's no
 * requirement that |len| be a multiple of any value and any partial blocks are
 * stored in |ecount_buf| and |*num|, which must be zeroed before the initial
 * call. The counter is a 128-bit, big-endian value in |ivec| and is
 * incremented by this function. */
OPENSSL_EXPORT void CRYPTO_ctr128_encrypt(const uint8_t *in, uint8_t *out,
                                          size_t len, const void *key,
                                          uint8_t ivec[16],
                                          uint8_t ecount_buf[16],
                                          unsigned int *num, block128_f block);

/* CRYPTO_ctr128_encrypt_ctr32 acts like |CRYPTO_ctr128_encrypt| but takes
 * |ctr|, a function that performs CTR mode but only deals with the lower 32
 * bits of the counter. This is useful when |ctr| can be an optimised
 * function. */
OPENSSL_EXPORT void CRYPTO_ctr128_encrypt_ctr32(
    const uint8_t *in, uint8_t *out, size_t len, const void *key,
    uint8_t ivec[16], uint8_t ecount_buf[16], unsigned int *num, ctr128_f ctr);


/* GCM. */

typedef struct gcm128_context GCM128_CONTEXT;

/* CRYPTO_gcm128_new allocates a fresh |GCM128_CONTEXT| and calls
 * |CRYPTO_gcm128_init|. It returns the new context, or NULL on error. */
OPENSSL_EXPORT GCM128_CONTEXT *CRYPTO_gcm128_new(void *key, block128_f block);

/* CRYPTO_gcm128_init initialises |ctx| to use |block| (typically AES) with the
 * given key. */
OPENSSL_EXPORT void CRYPTO_gcm128_init(GCM128_CONTEXT *ctx, void *key,
                                       block128_f block);

/* CRYPTO_gcm128_setiv sets the IV (nonce) for |ctx|. */
OPENSSL_EXPORT void CRYPTO_gcm128_setiv(GCM128_CONTEXT *ctx, const uint8_t *iv,
                                        size_t len);

/* CRYPTO_gcm128_aad sets the authenticated data for an instance of GCM. This
 * must be called before and data is encrypted. It returns one on success and
 * zero otherwise. */
OPENSSL_EXPORT int CRYPTO_gcm128_aad(GCM128_CONTEXT *ctx, const uint8_t *aad,
                                     size_t len);

/* CRYPTO_gcm128_encrypt encrypts |len| bytes from |in| to |out|. It returns
 * one on success and zero otherwise. */
OPENSSL_EXPORT int CRYPTO_gcm128_encrypt(GCM128_CONTEXT *ctx, const uint8_t *in,
                                         uint8_t *out, size_t len);

/* CRYPTO_gcm128_decrypt decrypts |len| bytes from |in| to |out|. It returns
 * one on success and zero otherwise. */
OPENSSL_EXPORT int CRYPTO_gcm128_decrypt(GCM128_CONTEXT *ctx, const uint8_t *in,
                                         uint8_t *out, size_t len);

/* CRYPTO_gcm128_encrypt_ctr32 encrypts |len| bytes from |in| to |out| using a
 * CTR function that only handles the bottom 32 bits of the nonce, like
 * |CRYPTO_ctr128_encrypt_ctr32|. It returns one on success and zero
 * otherwise. */
OPENSSL_EXPORT int CRYPTO_gcm128_encrypt_ctr32(GCM128_CONTEXT *ctx,
                                               const uint8_t *in, uint8_t *out,
                                               size_t len, ctr128_f stream);

/* CRYPTO_gcm128_decrypt_ctr32 decrypts |len| bytes from |in| to |out| using a
 * CTR function that only handles the bottom 32 bits of the nonce, like
 * |CRYPTO_ctr128_encrypt_ctr32|. It returns one on success and zero
 * otherwise. */
OPENSSL_EXPORT int CRYPTO_gcm128_decrypt_ctr32(GCM128_CONTEXT *ctx,
                                               const uint8_t *in, uint8_t *out,
                                               size_t len, ctr128_f stream);

/* CRYPTO_gcm128_finish calculates the authenticator and compares it against
 * |len| bytes of |tag|. It returns one on success and zero otherwise. */
OPENSSL_EXPORT int CRYPTO_gcm128_finish(GCM128_CONTEXT *ctx, const uint8_t *tag,
                                        size_t len);

/* CRYPTO_gcm128_tag calculates the authenticator and copies it into |tag|. The
 * minimum of |len| and 16 bytes are copied into |tag|. */
OPENSSL_EXPORT void CRYPTO_gcm128_tag(GCM128_CONTEXT *ctx, uint8_t *tag,
                                      size_t len);

/* CRYPTO_gcm128_release clears and frees |ctx|. */
OPENSSL_EXPORT void CRYPTO_gcm128_release(GCM128_CONTEXT *ctx);


/* CBC. */

/* cbc128_f is the type of a function that performs CBC-mode encryption. */
typedef void (*cbc128_f)(const uint8_t *in, uint8_t *out, size_t len,
                         const void *key, uint8_t ivec[16], int enc);

/* CRYPTO_cbc128_encrypt encrypts |len| bytes from |in| to |out| using the
 * given IV and block cipher in CBC mode. The input need not be a multiple of
 * 128 bits long, but the output will round up to the nearest 128 bit multiple,
 * zero padding the input if needed. The IV will be updated on return. */
void CRYPTO_cbc128_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                           const void *key, uint8_t ivec[16], block128_f block);

/* CRYPTO_cbc128_decrypt decrypts |len| bytes from |in| to |out| using the
 * given IV and block cipher in CBC mode. If |len| is not a multiple of 128
 * bits then only that many bytes will be written, but a multiple of 128 bits
 * is always read from |in|. The IV will be updated on return. */
void CRYPTO_cbc128_decrypt(const uint8_t *in, uint8_t *out, size_t len,
                           const void *key, uint8_t ivec[16], block128_f block);


/* OFB. */

/* CRYPTO_ofb128_encrypt encrypts (or decrypts, it's the same with OFB mode)
 * |len| bytes from |in| to |out| using |block| in OFB mode. There's no
 * requirement that |len| be a multiple of any value and any partial blocks are
 * stored in |ivec| and |*num|, the latter must be zero before the initial
 * call. */
void CRYPTO_ofb128_encrypt(const uint8_t *in, uint8_t *out,
                           size_t len, const void *key, uint8_t ivec[16],
                           int *num, block128_f block);


/* CFB. */

/* CRYPTO_cfb128_encrypt encrypts (or decrypts, if |enc| is zero) |len| bytes
 * from |in| to |out| using |block| in CFB mode. There's no requirement that
 * |len| be a multiple of any value and any partial blocks are stored in |ivec|
 * and |*num|, the latter must be zero before the initial call. */
void CRYPTO_cfb128_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                           const void *key, uint8_t ivec[16], int *num, int enc,
                           block128_f block);

/* CRYPTO_cfb128_8_encrypt encrypts (or decrypts, if |enc| is zero) |len| bytes
 * from |in| to |out| using |block| in CFB-8 mode. Prior to the first call
 * |num| should be set to zero. */
void CRYPTO_cfb128_8_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                             const void *key, uint8_t ivec[16], int *num,
                             int enc, block128_f block);

/* CRYPTO_cfb128_1_encrypt encrypts (or decrypts, if |enc| is zero) |len| bytes
 * from |in| to |out| using |block| in CFB-1 mode. Prior to the first call
 * |num| should be set to zero. */
void CRYPTO_cfb128_1_encrypt(const uint8_t *in, uint8_t *out, size_t bits,
                             const void *key, uint8_t ivec[16], int *num,
                             int enc, block128_f block);

size_t CRYPTO_cts128_encrypt_block(const uint8_t *in, uint8_t *out, size_t len,
                                   const void *key, uint8_t ivec[16],
                                   block128_f block);


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_MODES_H */
