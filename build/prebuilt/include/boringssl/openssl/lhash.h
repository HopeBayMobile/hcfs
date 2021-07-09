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

#ifndef OPENSSL_HEADER_LHASH_H
#define OPENSSL_HEADER_LHASH_H

#include <openssl/base.h>
#include <openssl/type_check.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* lhash is a traditional, chaining hash table that automatically expands and
 * contracts as needed. One should not use the lh_* functions directly, rather
 * use the type-safe macro wrappers:
 *
 * A hash table of a specific type of object has type |LHASH_OF(type)|. This
 * can be defined (once) with |DEFINE_LHASH_OF(type)| and declared where needed
 * with |DECLARE_LHASH_OF(type)|. For example:
 *
 *   struct foo {
 *     int bar;
 *   };
 *
 *   DEFINE_LHASH_OF(struct foo);
 *
 * Although note that the hash table will contain /pointers/ to |foo|.
 *
 * A macro will be defined for each of the lh_* functions below. For
 * LHASH_OF(foo), the macros would be lh_foo_new, lh_foo_num_items etc. */


#define LHASH_OF(type) struct lhash_st_##type

#define DEFINE_LHASH_OF(type) LHASH_OF(type) { int dummy; }

#define DECLARE_LHASH_OF(type) LHASH_OF(type);

/* The make_macros.sh script in this directory parses the following lines and
 * generates the lhash_macros.h file that contains macros for the following
 * types of stacks:
 *
 * LHASH_OF:ASN1_OBJECT
 * LHASH_OF:CONF_VALUE
 * LHASH_OF:SSL_SESSION */

#define IN_LHASH_H
#include <openssl/lhash_macros.h>
#undef IN_LHASH_H


/* lhash_item_st is an element of a hash chain. It points to the opaque data
 * for this element and to the next item in the chain. The linked-list is NULL
 * terminated. */
typedef struct lhash_item_st {
  void *data;
  struct lhash_item_st *next;
  /* hash contains the cached, hash value of |data|. */
  uint32_t hash;
} LHASH_ITEM;

/* lhash_cmp_func is a comparison function that returns a value equal, or not
 * equal, to zero depending on whether |*a| is equal, or not equal to |*b|,
 * respectively. Note the difference between this and |stack_cmp_func| in that
 * this takes pointers to the objects directly. */
typedef int (*lhash_cmp_func)(const void *a, const void *b);

/* lhash_hash_func is a function that maps an object to a uniformly distributed
 * uint32_t. */
typedef uint32_t (*lhash_hash_func)(const void *a);

typedef struct lhash_st {
  /* num_items contains the total number of items in the hash table. */
  size_t num_items;
  /* buckets is an array of |num_buckets| pointers. Each points to the head of
   * a chain of LHASH_ITEM objects that have the same hash value, mod
   * |num_buckets|. */
  LHASH_ITEM **buckets;
  /* num_buckets contains the length of |buckets|. This value is always >=
   * kMinNumBuckets. */
  size_t num_buckets;
  /* callback_depth contains the current depth of |lh_doall| or |lh_doall_arg|
   * calls. If non-zero then this suppresses resizing of the |buckets| array,
   * which would otherwise disrupt the iteration. */
  unsigned callback_depth;

  lhash_cmp_func comp;
  lhash_hash_func hash;
} _LHASH;

/* lh_new returns a new, empty hash table or NULL on error. If |comp| is NULL,
 * |strcmp| will be used. If |hash| is NULL, a generic hash function will be
 * used. */
OPENSSL_EXPORT _LHASH *lh_new(lhash_hash_func hash, lhash_cmp_func comp);

/* lh_free frees the hash table itself but none of the elements. See
 * |lh_doall|. */
OPENSSL_EXPORT void lh_free(_LHASH *lh);

/* lh_num_items returns the number of items in |lh|. */
OPENSSL_EXPORT size_t lh_num_items(const _LHASH *lh);

/* lh_retrieve finds an element equal to |data| in the hash table and returns
 * it. If no such element exists, it returns NULL. */
OPENSSL_EXPORT void *lh_retrieve(const _LHASH *lh, const void *data);

/* lh_insert inserts |data| into the hash table. If an existing element is
 * equal to |data| (with respect to the comparison function) then |*old_data|
 * will be set to that value and it will be replaced. Otherwise, or in the
 * event of an error, |*old_data| will be set to NULL. It returns one on
 * success or zero in the case of an allocation error. */
OPENSSL_EXPORT int lh_insert(_LHASH *lh, void **old_data, void *data);

/* lh_delete removes an element equal to |data| from the hash table and returns
 * it. If no such element is found, it returns NULL. */
OPENSSL_EXPORT void *lh_delete(_LHASH *lh, const void *data);

/* lh_doall calls |func| on each element of the hash table.
 * TODO(fork): rename this */
OPENSSL_EXPORT void lh_doall(_LHASH *lh, void (*func)(void *));

/* lh_doall_arg calls |func| on each element of the hash table and also passes
 * |arg| as the second argument.
 * TODO(fork): rename this */
OPENSSL_EXPORT void lh_doall_arg(_LHASH *lh, void (*func)(void *, void *),
                                 void *arg);

/* lh_strhash is the default hash function which processes NUL-terminated
 * strings. */
OPENSSL_EXPORT uint32_t lh_strhash(const char *c);


#if defined(__cplusplus)
} /* extern C */
#endif

#endif /* OPENSSL_HEADER_STACK_H */
