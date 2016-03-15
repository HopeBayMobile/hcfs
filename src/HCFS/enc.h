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
	int enc_alg;
	int comp_alg;
	char *enc_session_key;
	int len_enc_session_key;
} HCFS_encode_object_meta;

void free_object_meta(HCFS_encode_object_meta *object_meta);

int get_decode_meta(HCFS_encode_object_meta *, unsigned char *session_key,
		    unsigned char *key, int enc_flag, int compress_flag);

int generate_random_aes_key(unsigned char *);

int generate_random_bytes(unsigned char *, unsigned int);

int aes_gcm_encrypt_core(unsigned char *, unsigned char *, unsigned int,
			 unsigned char *, unsigned char *);

int aes_gcm_decrypt_core(unsigned char *, unsigned char *, unsigned int,
			 unsigned char *, unsigned char *);

int aes_gcm_encrypt_fix_iv(unsigned char *, unsigned char *, unsigned int,
			   unsigned char *);

int aes_gcm_decrypt_fix_iv(unsigned char *, unsigned char *, unsigned int,
			   unsigned char *);

int expect_b64_encode_length(unsigned int);

unsigned char *get_key(const char *);

FILE *transform_encrypt_fd(FILE *, unsigned char *, unsigned char **);

FILE *transform_fd(FILE *, unsigned char *, unsigned char **, int, int);

int decrypt_to_fd(FILE *, unsigned char *, unsigned char *, int);

int decode_to_fd(FILE *, unsigned char *, unsigned char *, int, int, int);

int decrypt_session_key(unsigned char *session_key, char *enc_session_key,
			unsigned char *key);

FILE *get_decrypt_configfp(const char *);

#endif /* SRC_HCFS_ENC_H_ */
