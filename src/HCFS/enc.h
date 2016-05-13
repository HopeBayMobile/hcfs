/*************************************************************************
 * *
 * * Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
 * *
 * * File Name: enc.h
 * * Abstract: The C header file for some encryption helpers
 * *
 * **************************************************************************/

#ifndef SRC_HCFS_ENC_H_
#define SRC_HCFS_ENC_H_

#include <string.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#ifdef OPENSSL_IS_BORINGSSL
#include <openssl/mem.h>
#endif
#include "params.h"
#include "b64encode.h"
#include "logger.h"
#include "compress.h"
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
