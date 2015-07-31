#include "enc.h"

/************************************************************************
 * *
 * * Function name: generate_random_bytes
 * *        Inputs: unsigned char* bytes: points to a buffer which
 *		    length should equals length
 *		    unsigned int length
 * *       Summary: generate some random bytes
 * *                https://www.openssl.org/docs/crypto/RAND_bytes.html
 * *
 * *  Return value: 0 if successful.
 *                  -1 if length <= 0.
 *                  -2 if random function not supported
 *                  -3 if some openssl error occurs, use ERR_get_error to get
 *                  error code
 * *
 * *************************************************************************/
int generate_random_bytes(unsigned char* bytes, unsigned int length){
	if(length <= 0)
		return -1;

	memset(bytes, 0, length);
	int rand_success = RAND_bytes(bytes, length);
	/* RAND_bytes() returns 1 on success, 0 otherwise. The error code can
	 * be obtained by ERR_get_error.
	 * return -1 if not supported by the current RAND method.
	 * https://www.openssl.org/docs/crypto/RAND_bytes.html
	 */
	switch(rand_success){
	case 1:
		return 0;
		break;
	case -1:
		return -2;
		break;
	default:
		return -3;
	}
}

/************************************************************************
 * *
 * * Function name: generate_random_aes_key
 * *        Inputs: unsigned char* key: points to a buffer which
 *		    length should equals KEY_SIZE
 * *       Summary: generate a random key for encryption
 * *
 * *  Return value: See get_random_bytes
 * *
 * *************************************************************************/
int generate_random_key(unsigned char* key){
	return generate_random_bytes(key, KEY_SIZE);
}

/************************************************************************
 * *
 * * Function name: aes_gcm_encrypt_fix_iv
 * *        Inputs: unsigned char* output which points to a buffer which
 *		    length should equals input_length + TAG_SIZE
 *		    unsigned char* input
 *		    unsigned int input_length
 *		    unsigned char* key
 * *       Summary: Use aes gcm mode to encrypt input (iv set to all zero)
 *		    It is dangerous if you use this function with a same key
 *		    again and again. So make sure you call this function
 *		    everytime with a different key.
 * *
 * *  Return value: 0 if successful. Otherwise returns error code.
 * *
 * *************************************************************************/
int aes_gcm_encrypt_fix_iv(unsigned char* output, unsigned char* input,
			   unsigned int input_length, unsigned char* key){
	int tmp_length = 0;
	int output_length = 0;
	int retcode = 0;
	unsigned char iv[IV_SIZE] = {0};
	unsigned char tag[TAG_SIZE] = {0};
	// preserve AUTH TAG spacee in 0 ~ (TAG_SIZE - 1)
	const int output_preserve_size = TAG_SIZE;

	// clear output
	memset(output, 0, input_length+TAG_SIZE);

	EVP_CIPHER_CTX ctx;
	EVP_CIPHER_CTX_init(&ctx);
	EVP_EncryptInit_ex(&ctx, EVP_aes_256_gcm(), NULL, key, iv);
	if(!EVP_EncryptUpdate(&ctx, output+output_preserve_size, &tmp_length,
			      input, input_length)){
		retcode = 1;
		goto final;
	}
	if(!EVP_EncryptFinal(&ctx, output+output_preserve_size+tmp_length,
			     &output_length)){
		retcode = 2;
		goto final;
	}
	if(!EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag)){
		retcode = 3;
		goto final;
	}
	memcpy(output, tag, TAG_SIZE);
	memset(tag, 0 , TAG_SIZE);
final:
	EVP_CIPHER_CTX_cleanup(&ctx);
	return retcode;
}

/************************************************************************
 * *
 * * Function name: aes_gcm_decrypt_fix_iv
 * *        Inputs: unsigned char* output which points to a buffer which
 *		    length should equals input_length - TAG_SIZE
 *		    unsigned char* input
 *		    unsigned int input_length
 *		    unsigned char* key
 * *       Summary: Use aes gcm mode to decrypt input (iv set to all zero)
 * *
 * *  Return value: 0 if successful. Otherwise returns error code.
 * *
 * *************************************************************************/
int aes_gcm_decrypt_fix_iv(unsigned char* output, unsigned char* input,
			   unsigned int input_length, unsigned char* key){
	int tmp_length = 0;
	int output_length = 0;
	int retcode = 0;
	unsigned char iv[IV_SIZE] = {0};
	unsigned char tag[TAG_SIZE] = {0};
	// AUTH TAG spacee in 0 ~ (TAG_SIZE - 1)
	const int preserve_size = TAG_SIZE;

	// clear output
	memset(output, 0, input_length-TAG_SIZE);

	EVP_CIPHER_CTX ctx;
	EVP_CIPHER_CTX_init(&ctx);
	EVP_DecryptInit_ex(&ctx, EVP_aes_256_gcm(), NULL, key, iv);
	if(!EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_SET_TAG, TAG_SIZE, input)){
		retcode = 3;
		goto decrypt_final;
	}
	if(!EVP_DecryptUpdate(&ctx, output, &tmp_length, input+preserve_size,
			  input_length-TAG_SIZE)){
		retcode = 1;
		goto decrypt_final;
	}
	if(!EVP_DecryptFinal(&ctx, tag, &output_length)){
		retcode = 2;
		goto decrypt_final;
	}
decrypt_final:
	EVP_CIPHER_CTX_cleanup(&ctx);
	return retcode;
}

int main(void){
	unsigned char key[KEY_SIZE];
	unsigned char input[1024] = {1};
	unsigned char output[1024+TAG_SIZE] = {0};
	unsigned char output_2[1024] = {0};
	generate_random_key(key);
	printf("key: %d\n", key[31]);
	int ret = aes_gcm_encrypt_fix_iv(output, input, 1024, key);
	printf("enc result: %d\n", output[31]);
	int i = 0;
	for(i = 0; i<TAG_SIZE; i++){
		printf("%d ", output[i]);
	}
	printf("\n");
	ret = aes_gcm_decrypt_fix_iv(output_2, output, 1024+TAG_SIZE, key);
	printf("dec result: %d\n", ret);
	printf("dec[0]: %d\n", output_2[0]);
	printf("dec[1]: %d\n", output_2[1]);

}
