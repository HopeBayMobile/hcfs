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

#ifndef OPENSSL_HEADER_THREAD_H
#define OPENSSL_HEADER_THREAD_H

#include <sys/types.h>

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


#if defined(OPENSSL_NO_THREADS)
typedef struct crypto_mutex_st {} CRYPTO_MUTEX;
#elif defined(OPENSSL_WINDOWS)
/* CRYPTO_MUTEX can appear in public header files so we really don't want to
 * pull in windows.h. It's statically asserted that this structure is large
 * enough to contain a Windows CRITICAL_SECTION by thread_win.c. */
typedef union crypto_mutex_st {
  double alignment;
  uint8_t padding[4*sizeof(void*) + 2*sizeof(int)];
} CRYPTO_MUTEX;
#elif defined(__MACH__) && defined(__APPLE__)
typedef pthread_rwlock_t CRYPTO_MUTEX;
#else
/* It is reasonable to include pthread.h on non-Windows systems, however the
 * |pthread_rwlock_t| that we need is hidden under feature flags, and we can't
 * ensure that we'll be able to get it. It's statically asserted that this
 * structure is large enough to contain a |pthread_rwlock_t| by
 * thread_pthread.c. */
typedef union crypto_mutex_st {
  double alignment;
  uint8_t padding[3*sizeof(int) + 5*sizeof(unsigned) + 16 + 8];
} CRYPTO_MUTEX;
#endif

/* CRYPTO_refcount_t is the type of a reference count.
 *
 * Since some platforms use C11 atomics to access this, it should have the
 * _Atomic qualifier. However, this header is included by C++ programs as well
 * as C code that might not set -std=c11. So, in practice, it's not possible to
 * do that. Instead we statically assert that the size and native alignment of
 * a plain uint32_t and an _Atomic uint32_t are equal in refcount_c11.c. */
typedef uint32_t CRYPTO_refcount_t;


/* Deprecated functions */

/* These defines do nothing but are provided to make old code easier to
 * compile. */
#define CRYPTO_LOCK 1
#define CRYPTO_UNLOCK 2
#define CRYPTO_READ 4
#define CRYPTO_WRITE 8

/* CRYPTO_num_locks returns one. (This is non-zero that callers who allocate
 * sizeof(lock) times this value don't get zero and then fail because malloc(0)
 * returned NULL.) */
OPENSSL_EXPORT int CRYPTO_num_locks(void);

/* CRYPTO_set_locking_callback does nothing. */
OPENSSL_EXPORT void CRYPTO_set_locking_callback(
    void (*func)(int mode, int lock_num, const char *file, int line));

/* CRYPTO_set_add_lock_callback does nothing. */
OPENSSL_EXPORT void CRYPTO_set_add_lock_callback(int (*func)(
    int *num, int amount, int lock_num, const char *file, int line));

/* CRYPTO_get_lock_name returns a fixed, dummy string. */
OPENSSL_EXPORT const char *CRYPTO_get_lock_name(int lock_num);

/* CRYPTO_THREADID_set_callback returns one. */
OPENSSL_EXPORT int CRYPTO_THREADID_set_callback(
    void (*threadid_func)(CRYPTO_THREADID *threadid));

/* CRYPTO_THREADID_set_numeric does nothing. */
OPENSSL_EXPORT void CRYPTO_THREADID_set_numeric(CRYPTO_THREADID *id,
                                                unsigned long val);

/* CRYPTO_THREADID_set_pointer does nothing. */
OPENSSL_EXPORT void CRYPTO_THREADID_set_pointer(CRYPTO_THREADID *id, void *ptr);

/* CRYPTO_THREADID_current does nothing. */
OPENSSL_EXPORT void CRYPTO_THREADID_current(CRYPTO_THREADID *id);


/* Private functions.
 *
 * Some old code calls these functions and so no-op implementations are
 * provided.
 *
 * TODO(fork): cleanup callers and remove. */

OPENSSL_EXPORT void CRYPTO_set_id_callback(unsigned long (*func)(void));

typedef struct {
  int references;
  struct CRYPTO_dynlock_value *data;
} CRYPTO_dynlock;

OPENSSL_EXPORT void CRYPTO_set_dynlock_create_callback(
    struct CRYPTO_dynlock_value *(*dyn_create_function)(const char *file,
                                                        int line));

OPENSSL_EXPORT void CRYPTO_set_dynlock_lock_callback(void (*dyn_lock_function)(
    int mode, struct CRYPTO_dynlock_value *l, const char *file, int line));

OPENSSL_EXPORT void CRYPTO_set_dynlock_destroy_callback(
    void (*dyn_destroy_function)(struct CRYPTO_dynlock_value *l,
                                 const char *file, int line));


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_THREAD_H */
