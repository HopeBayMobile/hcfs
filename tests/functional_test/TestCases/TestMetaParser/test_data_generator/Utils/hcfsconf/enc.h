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

#ifndef GW20_HCFSAPI_ENC_H_
#define GW20_HCFSAPI_ENC_H_

#include <string.h>
#include <inttypes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#ifdef OPENSSL_IS_BORINGSSL
#include <openssl/mem.h>
#endif

#define IV_SIZE 12
#define TAG_SIZE 16
#define KEY_SIZE 32


int32_t generate_random_aes_key(uint8_t *);

int32_t generate_random_bytes(uint8_t *, uint32_t);

int32_t aes_gcm_encrypt_core(uint8_t *, uint8_t *, uint32_t,
			 uint8_t *, uint8_t *);

int32_t aes_gcm_decrypt_core(uint8_t *, uint8_t *, uint32_t,
			 uint8_t *, uint8_t *);

uint8_t *get_key(char *);

#endif  /* GW20_HCFSAPI_ENC_H_ */
