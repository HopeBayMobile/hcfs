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

#ifndef OPENSSL_HEADER_TYPE_CHECK_H
#define OPENSSL_HEADER_TYPE_CHECK_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* This header file contains some common macros for enforcing type checking.
 * Several, common OpenSSL structures (i.e. stack and lhash) operate on void
 * pointers, but we wish to have type checking when they are used with a
 * specific type. */

/* CHECKED_CAST casts |p| from type |from| to type |to|. */
#define CHECKED_CAST(to, from, p) ((to) (1 ? (p) : (from)0))

/* CHECKED_PTR_OF casts a given pointer to void* and statically checks that it
 * was a pointer to |type|. */
#define CHECKED_PTR_OF(type, p) CHECKED_CAST(void*, type*, (p))

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define OPENSSL_COMPILE_ASSERT(cond, msg) _Static_assert(cond, #msg)
#else
#define OPENSSL_COMPILE_ASSERT(cond, msg) \
  typedef char OPENSSL_COMPILE_ASSERT_##msg[((cond) ? 1 : -1)]
#endif


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_TYPE_CHECK_H */
