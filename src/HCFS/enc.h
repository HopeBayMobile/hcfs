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

#ifndef SRC_HCFS_ENC_H_
#define SRC_HCFS_ENC_H_

#include <string.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <inttypes.h>
#ifdef OPENSSL_IS_BORINGSSL
#include <openssl/mem.h>
#endif
#define IV_SIZE 12
#define TAG_SIZE 16
#define KEY_SIZE 32

/* Object Metadata */
#define ENC_ALG_V1 1
#define COMP_ALG_V1 1

#define ENC_ALG_NONE 0
#define COMP_ALG_NONE 0

typedef struct encode_object_meta {
	int32_t enc_alg;
	int32_t comp_alg;
	char *enc_session_key;
	int32_t len_enc_session_key;
} HCFS_encode_object_meta;

void free_object_meta(HCFS_encode_object_meta *object_meta);

int32_t get_decode_meta(HCFS_encode_object_meta *, uint8_t *session_key,
		    uint8_t *key, int32_t enc_flag, int32_t compress_flag);

int32_t generate_random_aes_key(uint8_t *);

int32_t generate_random_bytes(uint8_t *, uint32_t);

int32_t aes_gcm_encrypt_core(uint8_t *, uint8_t *, uint32_t,
			 uint8_t *, uint8_t *);

int32_t aes_gcm_decrypt_core(uint8_t *, uint8_t *, uint32_t,
			 uint8_t *, uint8_t *);

int32_t aes_gcm_encrypt_fix_iv(uint8_t *, uint8_t *, uint32_t,
			   uint8_t *);

int32_t aes_gcm_decrypt_fix_iv(uint8_t *, uint8_t *, uint32_t,
			   uint8_t *);

int32_t expect_b64_encode_length(uint32_t);

uint8_t *get_key(const char *);

FILE *transform_encrypt_fd(FILE *, uint8_t *, uint8_t **);

FILE *transform_fd(FILE *, uint8_t *, uint8_t **, int32_t, int32_t);

int32_t decrypt_to_fd(FILE *, uint8_t *, uint8_t *, int32_t);

int32_t decode_to_fd(FILE *, uint8_t *, uint8_t *, int32_t, int32_t, int32_t);

int32_t decrypt_session_key(uint8_t *session_key, char *enc_session_key,
			uint8_t *key);

FILE *get_decrypt_configfp(const char *);

int32_t enc_backup_usermeta(char *json_str);
char *dec_backup_usermeta(char *path);
#endif /* GW20_HCFS_ENC_H_ */
