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

#ifndef OPENSSL_HEADER_RAND_H
#define OPENSSL_HEADER_RAND_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* Random number generation. */


/* RAND_bytes writes |len| bytes of random data to |buf| and returns one. */
OPENSSL_EXPORT int RAND_bytes(uint8_t *buf, size_t len);

/* RAND_cleanup frees any resources used by the RNG. This is not safe if other
 * threads might still be calling |RAND_bytes|. */
OPENSSL_EXPORT void RAND_cleanup(void);


/* Deprecated functions */

/* RAND_pseudo_bytes is a wrapper around |RAND_bytes|. */
OPENSSL_EXPORT int RAND_pseudo_bytes(uint8_t *buf, size_t len);

/* RAND_seed does nothing. */
OPENSSL_EXPORT void RAND_seed(const void *buf, int num);

/* RAND_load_file returns a nonnegative number. */
OPENSSL_EXPORT int RAND_load_file(const char *path, long num);

/* RAND_add does nothing. */
OPENSSL_EXPORT void RAND_add(const void *buf, int num, double entropy);

/* RAND_poll returns one. */
OPENSSL_EXPORT int RAND_poll(void);

/* RAND_status returns one. */
OPENSSL_EXPORT int RAND_status(void);


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_RAND_H */
