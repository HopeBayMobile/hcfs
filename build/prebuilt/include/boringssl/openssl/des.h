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

#ifndef OPENSSL_HEADER_DES_H
#define OPENSSL_HEADER_DES_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* DES. */


typedef struct DES_cblock_st {
  uint8_t bytes[8];
} DES_cblock;

typedef struct DES_ks {
  union {
    DES_cblock cblock;
    /* make sure things are correct size on machines with
     * 8 byte longs */
    uint32_t deslong[2];
  } ks[16];
} DES_key_schedule;


#define DES_KEY_SZ (sizeof(DES_cblock))
#define DES_SCHEDULE_SZ (sizeof(DES_key_schedule))

#define DES_ENCRYPT 1
#define DES_DECRYPT 0

#define DES_CBC_MODE 0
#define DES_PCBC_MODE 1

/* DES_set_key performs a key schedule and initialises |schedule| with |key|. */
OPENSSL_EXPORT void DES_set_key(const DES_cblock *key,
                                DES_key_schedule *schedule);

/* DES_set_odd_parity sets the parity bits (the least-significant bits in each
 * byte) of |key| given the other bits in each byte. */
OPENSSL_EXPORT void DES_set_odd_parity(DES_cblock *key);

/* DES_ecb_encrypt encrypts (or decrypts, if |is_encrypt| is |DES_DECRYPT|) a
 * single DES block (8 bytes) from in to out, using the key configured in
 * |schedule|. */
OPENSSL_EXPORT void DES_ecb_encrypt(const DES_cblock *in, DES_cblock *out,
                                    const DES_key_schedule *schedule,
                                    int is_encrypt);

/* DES_ncbc_encrypt encrypts (or decrypts, if |enc| is |DES_DECRYPT|) |len|
 * bytes from |in| to |out| with DES in CBC mode. */
OPENSSL_EXPORT void DES_ncbc_encrypt(const uint8_t *in, uint8_t *out,
                                     size_t len,
                                     const DES_key_schedule *schedule,
                                     DES_cblock *ivec, int enc);

/* DES_ecb3_encrypt encrypts (or decrypts, if |enc| is |DES_DECRYPT|) a single
 * block (8 bytes) of data from |input| to |output| using 3DES. */
OPENSSL_EXPORT void DES_ecb3_encrypt(const DES_cblock *input,
                                     DES_cblock *output,
                                     const DES_key_schedule *ks1,
                                     const DES_key_schedule *ks2,
                                     const DES_key_schedule *ks3,
                                     int enc);

/* DES_ede3_cbc_encrypt encrypts (or decrypts, if |enc| is |DES_DECRYPT|) |len|
 * bytes from |in| to |out| with 3DES in CBC mode. 3DES uses three keys, thus
 * the function takes three different |DES_key_schedule|s. */
OPENSSL_EXPORT void DES_ede3_cbc_encrypt(const uint8_t *in, uint8_t *out,
                                         size_t len,
                                         const DES_key_schedule *ks1,
                                         const DES_key_schedule *ks2,
                                         const DES_key_schedule *ks3,
                                         DES_cblock *ivec, int enc);

/* DES_ede2_cbc_encrypt encrypts (or decrypts, if |enc| is |DES_DECRYPT|) |len|
 * bytes from |in| to |out| with 3DES in CBC mode. With this keying option, the
 * first and third 3DES keys are identical. Thus, this function takes only two
 * different |DES_key_schedule|s. */
OPENSSL_EXPORT void DES_ede2_cbc_encrypt(const uint8_t *in, uint8_t *out,
                                         size_t len,
                                         const DES_key_schedule *ks1,
                                         const DES_key_schedule *ks2,
                                         DES_cblock *ivec, int enc);


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_DES_H */
