/*************************************************************************
 * *
 * * Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
 * *
 * * File Name: enc.h
 * * Abstract: The C header file for some encryption helpers
 * *
 * **************************************************************************/

#ifndef GW20_HCFS_ENC_H_
#define GW20_HCFS_ENC_H_

#include <string.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include "b64encode.h"
#define IV_SIZE 12
#define TAG_SIZE 16
#define KEY_SIZE 32
#define MAX_ENC_DATA 3145728

int generate_random_key(unsigned char *);

int generate_random_bytes(unsigned char *, unsigned int);

int aes_gcm_encrypt_core(unsigned char *, unsigned char *, unsigned int,
			 unsigned char *, unsigned char *);

int aes_gcm_decrypt_core(unsigned char *, unsigned char *, unsigned int,
			 unsigned char *, unsigned char *);

int aes_gcm_encrypt_fix_iv(unsigned char *, unsigned char *, unsigned int,
			   unsigned char *);

int aes_gcm_decrypt_fix_iv(unsigned char *, unsigned char*, unsigned int,
			   unsigned char *);

int expect_b64_encode_length(unsigned int);

unsigned char *get_key(void);

FILE *transform_encrypt_fd(FILE *, unsigned char *, unsigned char **);

int decrypt_to_fd(FILE *, unsigned char *, unsigned char *, int);


#endif  /* GW20_HCFS_ENC_H_ */
