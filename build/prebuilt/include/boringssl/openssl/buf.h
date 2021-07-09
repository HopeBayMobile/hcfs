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

#ifndef OPENSSL_HEADER_BUFFER_H
#define OPENSSL_HEADER_BUFFER_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* Memory and string functions, see also mem.h. */


/* BUF_MEM is a generic buffer object used by OpenSSL. */
struct buf_mem_st {
  size_t length; /* current number of bytes */
  char *data;
  size_t max; /* size of buffer */
};

/* BUF_MEM_new creates a new BUF_MEM which has no allocated data buffer. */
OPENSSL_EXPORT BUF_MEM *BUF_MEM_new(void);

/* BUF_MEM_free frees |buf->data| if needed and then frees |buf| itself. */
OPENSSL_EXPORT void BUF_MEM_free(BUF_MEM *buf);

/* BUF_MEM_grow ensures that |buf| has length |len| and allocates memory if
 * needed. If the length of |buf| increased, the new bytes are filled with
 * zeros. It returns the length of |buf|, or zero if there's an error. */
OPENSSL_EXPORT size_t BUF_MEM_grow(BUF_MEM *buf, size_t len);

/* BUF_MEM_grow_clean acts the same as |BUF_MEM_grow|, but clears the previous
 * contents of memory if reallocing. */
OPENSSL_EXPORT size_t BUF_MEM_grow_clean(BUF_MEM *str, size_t len);

/* BUF_strdup returns an allocated, duplicate of |str|. */
OPENSSL_EXPORT char *BUF_strdup(const char *str);

/* BUF_strnlen returns the number of characters in |str|, excluding the NUL
 * byte, but at most |max_len|. This function never reads more than |max_len|
 * bytes from |str|. */
OPENSSL_EXPORT size_t BUF_strnlen(const char *str, size_t max_len);

/* BUF_strndup returns an allocated, duplicate of |str|, which is, at most,
 * |size| bytes. The result is always NUL terminated. */
OPENSSL_EXPORT char *BUF_strndup(const char *str, size_t size);

/* BUF_memdup returns an allocated, duplicate of |size| bytes from |data|. */
OPENSSL_EXPORT void *BUF_memdup(const void *data, size_t size);

/* BUF_strlcpy acts like strlcpy(3). */
OPENSSL_EXPORT size_t BUF_strlcpy(char *dst, const char *src, size_t dst_size);

/* BUF_strlcat acts like strlcat(3). */
OPENSSL_EXPORT size_t BUF_strlcat(char *dst, const char *src, size_t size);


#if defined(__cplusplus)
}  /* extern C */
#endif

#define BUF_F_BUF_MEM_new 100
#define BUF_F_BUF_memdup 101
#define BUF_F_BUF_strndup 102
#define BUF_F_buf_mem_grow 103

#endif  /* OPENSSL_HEADER_BUFFER_H */
