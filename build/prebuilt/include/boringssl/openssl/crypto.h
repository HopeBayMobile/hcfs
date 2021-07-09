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

#ifndef OPENSSL_HEADER_CRYPTO_H
#define OPENSSL_HEADER_CRYPTO_H

#include <openssl/base.h>

/* Upstream OpenSSL defines |OPENSSL_malloc|, etc., in crypto.h rather than
 * mem.h. */
#include <openssl/mem.h>


#if defined(__cplusplus)
extern "C" {
#endif


/* crypto.h contains functions for initializing the crypto library. */


/* CRYPTO_library_init initializes the crypto library. It must be called if the
 * library is built with BORINGSSL_NO_STATIC_INITIALIZER. Otherwise, it does
 * nothing and a static initializer is used instead. */
OPENSSL_EXPORT void CRYPTO_library_init(void);


/* Deprecated functions. */

#define OPENSSL_VERSION_TEXT "BoringSSL"

#define SSLEAY_VERSION 0

/* SSLeay_version is a compatibility function that returns the string
 * "BoringSSL". */
OPENSSL_EXPORT const char *SSLeay_version(int unused);

/* SSLeay is a compatibility function that returns OPENSSL_VERSION_NUMBER from
 * base.h. */
OPENSSL_EXPORT unsigned long SSLeay(void);


#if defined(__cplusplus)
}  /* extern C */
#endif

#define CRYPTO_F_CRYPTO_get_ex_new_index 100
#define CRYPTO_F_CRYPTO_set_ex_data 101
#define CRYPTO_F_get_class 102
#define CRYPTO_F_get_func_pointers 103

#endif  /* OPENSSL_HEADER_CRYPTO_H */
