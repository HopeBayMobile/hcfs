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

#ifndef OPENSSL_HEADER_MEM_H
#define OPENSSL_HEADER_MEM_H

#include <openssl/base.h>

#include <stdlib.h>
#include <stdarg.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* Memory and string functions, see also buf.h.
 *
 * OpenSSL has, historically, had a complex set of malloc debugging options.
 * However, that was written in a time before Valgrind and ASAN. Since we now
 * have those tools, the OpenSSL allocation functions are simply macros around
 * the standard memory functions. */


#define OPENSSL_malloc malloc
#define OPENSSL_realloc realloc
#define OPENSSL_free free

/* OPENSSL_realloc_clean acts like |realloc|, but clears the previous memory
 * buffer.  Because this is implemented as a wrapper around |malloc|, it needs
 * to be given the size of the buffer pointed to by |ptr|. */
void *OPENSSL_realloc_clean(void *ptr, size_t old_size, size_t new_size);

/* OPENSSL_cleanse zeros out |len| bytes of memory at |ptr|. This is similar to
 * |memset_s| from C11. */
OPENSSL_EXPORT void OPENSSL_cleanse(void *ptr, size_t len);

/* CRYPTO_memcmp returns zero iff the |len| bytes at |a| and |b| are equal. It
 * takes an amount of time dependent on |len|, but independent of the contents
 * of |a| and |b|. Unlike memcmp, it cannot be used to put elements into a
 * defined order as the return value when a != b is undefined, other than to be
 * non-zero. */
OPENSSL_EXPORT int CRYPTO_memcmp(const void *a, const void *b, size_t len);

/* OPENSSL_hash32 implements the 32 bit, FNV-1a hash. */
OPENSSL_EXPORT uint32_t OPENSSL_hash32(const void *ptr, size_t len);

/* OPENSSL_strdup has the same behaviour as strdup(3). */
OPENSSL_EXPORT char *OPENSSL_strdup(const char *s);

/* OPENSSL_strnlen has the same behaviour as strnlen(3). */
OPENSSL_EXPORT size_t OPENSSL_strnlen(const char *s, size_t len);

/* OPENSSL_strcasecmp has the same behaviour as strcasecmp(3). */
OPENSSL_EXPORT int OPENSSL_strcasecmp(const char *a, const char *b);

/* OPENSSL_strncasecmp has the same behaviour as strncasecmp(3). */
OPENSSL_EXPORT int OPENSSL_strncasecmp(const char *a, const char *b, size_t n);

/* DECIMAL_SIZE returns an upper bound for the length of the decimal
 * representation of the given type. */
#define DECIMAL_SIZE(type)	((sizeof(type)*8+2)/3+1)

/* Printf functions.
 *
 * These functions are either OpenSSL wrappers for standard functions (i.e.
 * |BIO_snprintf| and |BIO_vsnprintf|) which don't exist in C89, or are
 * versions of printf functions that output to a BIO rather than a FILE. */
#ifdef __GNUC__
#define __bio_h__attr__ __attribute__
#else
#define __bio_h__attr__(x)
#endif
OPENSSL_EXPORT int BIO_snprintf(char *buf, size_t n, const char *format, ...)
    __bio_h__attr__((__format__(__printf__, 3, 4)));

OPENSSL_EXPORT int BIO_vsnprintf(char *buf, size_t n, const char *format,
                                 va_list args)
    __bio_h__attr__((__format__(__printf__, 3, 0)));
#undef __bio_h__attr__


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_MEM_H */
