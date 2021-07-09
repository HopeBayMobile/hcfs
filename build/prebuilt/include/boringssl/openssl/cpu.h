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

#ifndef OPENSSL_HEADER_CPU_H
#define OPENSSL_HEADER_CPU_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* Runtime CPU feature support */


#if defined(OPENSSL_X86) || defined(OPENSSL_X86_64)
/* OPENSSL_ia32cap_P contains the Intel CPUID bits when running on an x86 or
 * x86-64 system.
 *
 *   Index 0:
 *     EDX for CPUID where EAX = 1
 *     Bit 30 is used to indicate an Intel CPU
 *   Index 1:
 *     ECX for CPUID where EAX = 1
 *   Index 2:
 *     EBX for CPUID where EAX = 7
 *
 * Note: the CPUID bits are pre-adjusted for the OSXSAVE bit and the YMM and XMM
 * bits in XCR0, so it is not necessary to check those. */
extern uint32_t OPENSSL_ia32cap_P[4];
#endif

#if defined(OPENSSL_ARM) || defined(OPENSSL_AARCH64)
/* CRYPTO_is_NEON_capable returns true if the current CPU has a NEON unit. Note
 * that |OPENSSL_armcap_P| also exists and contains the same information in a
 * form that's easier for assembly to use. */
OPENSSL_EXPORT char CRYPTO_is_NEON_capable(void);

/* CRYPTO_set_NEON_capable sets the return value of |CRYPTO_is_NEON_capable|.
 * By default, unless the code was compiled with |-mfpu=neon|, NEON is assumed
 * not to be present. It is not autodetected. Calling this with a zero
 * argument also causes |CRYPTO_is_NEON_functional| to return false. */
OPENSSL_EXPORT void CRYPTO_set_NEON_capable(char neon_capable);

/* CRYPTO_is_NEON_functional returns true if the current CPU has a /working/
 * NEON unit. Some phones have a NEON unit, but the Poly1305 NEON code causes
 * it to fail. See https://code.google.com/p/chromium/issues/detail?id=341598 */
OPENSSL_EXPORT char CRYPTO_is_NEON_functional(void);

/* CRYPTO_set_NEON_functional sets the "NEON functional" flag. For
 * |CRYPTO_is_NEON_functional| to return true, both this flag and the NEON flag
 * must be true. By default NEON is assumed to be functional if the code was
 * compiled with |-mfpu=neon| or if |CRYPTO_set_NEON_capable| has been called
 * with a non-zero argument. */
OPENSSL_EXPORT void CRYPTO_set_NEON_functional(char neon_functional);
#endif  /* OPENSSL_ARM */


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_CPU_H */
