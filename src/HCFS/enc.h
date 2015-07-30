#include <string.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#define IV_SIZE 12
#define TAG_SIZE 16
#define KEY_SIZE 32

int generate_random_key(unsigned char*);

int aes_gcm_encrypt_fix_iv(unsigned char*, unsigned char*, unsigned int,
			   unsigned char*);

int aes_gcm_decrypt_fix_iv(unsigned char*, unsigned char*, unsigned int,
			   unsigned char*);
