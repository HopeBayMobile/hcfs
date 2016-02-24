/*************************************************************************
 * *
 * * Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
 * *
 * * File Name: enc.h
 * * Abstract: The C header file for some encryption helpers
 * *
 * **************************************************************************/

#include <string.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#ifdef OPENSSL_IS_BORINGSSL
#include <openssl/mem.h>
#endif
#define IV_SIZE 12
#define TAG_SIZE 16
#define KEY_SIZE 32


int generate_random_aes_key(unsigned char *);

int generate_random_bytes(unsigned char *, unsigned int);

int aes_gcm_encrypt_core(unsigned char *, unsigned char *, unsigned int,
			 unsigned char *, unsigned char *);

int aes_gcm_decrypt_core(unsigned char *, unsigned char *, unsigned int,
			 unsigned char *, unsigned char *);

unsigned char *get_key(char *);

