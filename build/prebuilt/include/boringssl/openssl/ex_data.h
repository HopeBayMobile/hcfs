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
/* ====================================================================
 * Copyright (c) 1998-2001 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com). */

#ifndef OPENSSL_HEADER_EX_DATA_H
#define OPENSSL_HEADER_EX_DATA_H

#include <openssl/base.h>

#include <openssl/stack.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* ex_data is a mechanism for associating arbitrary extra data with objects.
 * For each type of object that supports ex_data, different users can be
 * assigned indexes in which to store their data. Each index has callback
 * functions that are called when a new object of that type is created, freed
 * and duplicated. */


typedef struct crypto_ex_data_st CRYPTO_EX_DATA;


/* Type-specific functions.
 *
 * Each type that supports ex_data provides three functions: */

#if 0 /* Sample */

/* |TYPE_get_ex_new_index| allocates a new index for |TYPE|. See the
 * descriptions of the callback typedefs for details of when they are
 * called. Any of the callback arguments may be NULL. The |argl| and |argp|
 * arguments are opaque values that are passed to the callbacks. It returns the
 * new index or a negative number on error.
 *
 * TODO(fork): this should follow the standard calling convention. */
OPENSSL_EXPORT int TYPE_get_ex_new_index(long argl, void *argp,
                                         CRYPTO_EX_new *new_func,
                                         CRYPTO_EX_dup *dup_func,
                                         CRYPTO_EX_free *free_func);

/* |TYPE_set_ex_data| sets an extra data pointer on |t|. The |index| argument
 * should have been returned from a previous call to |TYPE_get_ex_new_index|. */
OPENSSL_EXPORT int TYPE_set_ex_data(TYPE *t, int index, void *arg);

/* |TYPE_get_ex_data| returns an extra data pointer for |t|, or NULL if no such
 * pointer exists. The |index| argument should have been returned from a
 * previous call to |TYPE_get_ex_new_index|. */
OPENSSL_EXPORT void *TYPE_get_ex_data(const TYPE *t, int index);

#endif /* Sample */


/* Callback types. */

/* CRYPTO_EX_new is the type of a callback function that is called whenever a
 * new object of a given class is created. For example, if this callback has
 * been passed to |SSL_get_ex_new_index| then it'll be called each time an SSL*
 * is created.
 *
 * The callback is passed the new object (i.e. the SSL*) in |parent|. The
 * arguments |argl| and |argp| contain opaque values that were given to
 * |CRYPTO_get_ex_new_index|. The callback should return one on success, but
 * the value is ignored.
 *
 * TODO(fork): the |ptr| argument is always NULL, no? */
typedef int CRYPTO_EX_new(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
                          int index, long argl, void *argp);

/* CRYPTO_EX_free is a callback function that is called when an object of the
 * class is being destroyed. See |CRYPTO_EX_new| for a discussion of the
 * arguments.
 *
 * If |CRYPTO_get_ex_new_index| was called after the creation of objects of the
 * class that this applies to then, when those those objects are destroyed,
 * this callback will be called with a NULL value for |ptr|. */
typedef void CRYPTO_EX_free(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
                            int index, long argl, void *argp);

/* CRYPTO_EX_dup is a callback function that is called when an object of the
 * class is being copied and thus the ex_data linked to it also needs to be
 * copied. On entry, |*from_d| points to the data for this index from the
 * original object. When the callback returns, |*from_d| will be set as the
 * data for this index in |to|.
 *
 * If |CRYPTO_get_ex_new_index| was called after the creation of objects of the
 * class that this applies to then, when those those objects are copies, this
 * callback will be called with a NULL value for |*from_d|. */
typedef int CRYPTO_EX_dup(CRYPTO_EX_DATA *to, const CRYPTO_EX_DATA *from,
                          void **from_d, int index, long argl, void *argp);


/* Deprecated functions. */

/* CRYPTO_cleanup_all_ex_data does nothing. */
OPENSSL_EXPORT void CRYPTO_cleanup_all_ex_data(void);

struct crypto_ex_data_st {
  STACK_OF(void) *sk;
};


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_EX_DATA_H */
