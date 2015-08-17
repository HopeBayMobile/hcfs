/*************************************************************************
 * *
 * * Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
 * *
 * * File Name: enc.c
 * * Abstract: The c source code file for some encryption helpers.
 * *
 * **************************************************************************/


#include "enc.h"


/************************************************************************
 * *
 * * Function name: expect_b64_encode_length
 * *        Inputs: int length
 * *       Summary: Calculate how many bytes will be after encode
 * *
 * *  Return value: bytes expected
 * *
 * *************************************************************************/
int expect_b64_encode_length(unsigned int length) {
	int tmp = length % 3;
	tmp = (tmp == 0)? tmp : (3-tmp);
	// 1 is for b64encode_str puts '\0' in the end
	return  1+(length+tmp)*4/3;
}

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
 *                  -2 if openssl RAND not supported
 *                  -3 if some openssl error occurs, use ERR_get_error to get
 *                  error code
 * *
 * *************************************************************************/
int generate_random_bytes(unsigned char* bytes, unsigned int length) {
	if(length <= 0)
		return -1;

	memset(bytes, 0, length);
	int rand_success = RAND_bytes(bytes, length);
	/* RAND_bytes() returns 1 on success, 0 otherwise. The error code can
	 * be obtained by ERR_get_error.
	 * return -1 if not supported by the current RAND method.
	 * https://www.openssl.org/docs/crypto/RAND_bytes.html
	 */
	switch(rand_success) {
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
int generate_random_key(unsigned char* key) {
	return generate_random_bytes(key, KEY_SIZE);
}


/************************************************************************
 * *
 * * Function name: aes_gcm_encrypt_core
 * *        Inputs: unsigned char* output: points to a buffer which
 *		                           length should equals input_length + TAG_SIZE
 *		    unsigned char* input
 *		    unsigned int input_length
 *		    unsigned char* key: must be KEY_SIZE length
 *		    unsigned char* iv: must be IV_SIZE length
 * *       Summary: Use aes gcm mode to encrypt input
 * *
 * *  Return value: 0 if successful.
 *                  1 if Encrypt Update error
 *                  2 if Encrypt Final error
 *                  3 if extract TAG error
 * *
 * *************************************************************************/
int aes_gcm_encrypt_core(unsigned char* output, unsigned char* input,
		   unsigned int input_length, unsigned char* key,
		   unsigned char* iv) {
	int tmp_length = 0;
	int output_length = 0;
	int retcode = 0;
	const int output_preserve_size = TAG_SIZE;
	unsigned char tag[TAG_SIZE] = {0};
	// clear output
	memset(output, 0, input_length+TAG_SIZE);
	EVP_CIPHER_CTX ctx;
	EVP_CIPHER_CTX_init(&ctx);
	EVP_EncryptInit_ex(&ctx, EVP_aes_256_gcm(), NULL, key, iv);
	if(!EVP_EncryptUpdate(&ctx, output+output_preserve_size, &tmp_length,
			      input, input_length)) {
		retcode = 1;
		goto final;
	}
	if(!EVP_EncryptFinal(&ctx, output+output_preserve_size+tmp_length,
			     &output_length)) {
		retcode = 2;
		goto final;
	}
	if(!EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag)) {
		retcode = 3;
		goto final;
	}
	memcpy(output, tag, TAG_SIZE);
	memset(tag, 0, TAG_SIZE);
final:
	EVP_CIPHER_CTX_cleanup(&ctx);
	return retcode;
}

/************************************************************************
 * *
 * * Function name: aes_gcm_decrypt_core
 * *        Inputs: unsigned char* output:  points to a buffer which
 *		    length should equals input_length - TAG_SIZE
 *		    unsigned char* input
 *		    unsigned int input_length
 *		    unsigned char* key: must KEY_SIZE length
 *		    unsigned char* iv: must IV_SIZE length
 * *       Summary: Use aes gcm mode to decrypt input
 * *
 * *  Return value: 0 if successful.
 *                  3 if set reference TAG error
 *                  1 if Decrypt update error
 *                  2 if Decrypr final error (TAG wrong)
 * *
 * *************************************************************************/
int aes_gcm_decrypt_core(unsigned char* output, unsigned char* input,
			 unsigned int input_length, unsigned char* key,
			 unsigned char* iv) {
	int tmp_length = 0;
	int output_length = 0;
	int retcode = 0;
	unsigned char tag[TAG_SIZE] = {0};
	const int preserve_size = TAG_SIZE;

	// clear output
	memset(output, 0, input_length-TAG_SIZE);

	EVP_CIPHER_CTX ctx;
	EVP_CIPHER_CTX_init(&ctx);
	EVP_DecryptInit_ex(&ctx, EVP_aes_256_gcm(), NULL, key, iv);
	if(!EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_SET_TAG, TAG_SIZE, input)) {
		retcode = 3;
		goto decrypt_final;
	}
	if(!EVP_DecryptUpdate(&ctx, output, &tmp_length, input+preserve_size,
			  input_length-TAG_SIZE)) {
		retcode = 1;
		goto decrypt_final;
	}
	if(!EVP_DecryptFinal(&ctx, tag, &output_length)) {
		retcode = 2;
		goto decrypt_final;
	}
decrypt_final:
	EVP_CIPHER_CTX_cleanup(&ctx);
	return retcode;
}

/************************************************************************
 * *
 * * Function name: aes_gcm_encrypt_fix_iv
 * *        Inputs: unsigned char* output: points to a buffer which
 *		    length should equals input_length + TAG_SIZE
 *		    unsigned char* input
 *		    unsigned int input_length
 *		    unsigned char* key
 * *       Summary: Use aes gcm mode to encrypt input (iv set to all zero)
 *		    It is dangerous if you use this function with a same key
 *		    again and again. So make sure you call this function
 *		    everytime with a different key.
 * *
 * *  Return value: See aes_gcm_encrypt_core
 * *
 * *************************************************************************/
int aes_gcm_encrypt_fix_iv(unsigned char* output, unsigned char* input,
			   unsigned int input_length, unsigned char* key) {
	unsigned char iv[IV_SIZE] = {0};
	return aes_gcm_encrypt_core(output, input, input_length, key, iv);
}

/************************************************************************
 * *
 * * Function name: aes_gcm_decrypt_fix_iv
 * *        Inputs: unsigned char* output: points to a buffer which
 *		    length should equals input_length - TAG_SIZE
 *		    unsigned char* input
 *		    unsigned int input_length
 *		    unsigned char* key
 * *       Summary: Use aes gcm mode to decrypt input (iv set to all zero)
 * *
 * *  Return value: See aes_gcm_decrypt_core
 * *
 * *************************************************************************/
int aes_gcm_decrypt_fix_iv(unsigned char* output, unsigned char* input,
			   unsigned int input_length, unsigned char* key) {
	unsigned char iv[IV_SIZE] = {0};
	return aes_gcm_decrypt_core(output, input, input_length, key, iv);
}

/*
 * This function only for deceloping upload to cloud.
 * In the future, it should be reimplemented considering
 * key management specs
 */
unsigned char* get_key() {
	const char* user_pass = "this is hopebay testing";
	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;
	unsigned char *ret = (unsigned char*)calloc(KEY_SIZE,
						    sizeof(unsigned char));
	if (!ret) return NULL;
	const EVP_MD *m;
	EVP_MD_CTX ctx;
	m = EVP_sha256();
	if (!m)
		return NULL;
	EVP_DigestInit(&ctx, m);
	unsigned char* salt = (unsigned
			       char*)"oluik.354jhmnk,";
	PKCS5_PBKDF2_HMAC(user_pass,
			  strlen(user_pass), salt,
			  strlen((char*)salt), 3,
			  m, KEY_SIZE, ret);
	EVP_DigestFinal(&ctx, md_value,
			&md_len);
	return ret;
}

/************************************************************************
 * *
 * * Function name: transform_encrypt_fd
 * *        Inputs: FILE* in_fd, open with 'r' mode
 *		    unsigned char* key
 *		    unsigned char** data
 * *       Summary: Encrypt content read from in_fd, and return a new fd
 *		    data must be free outside this function
 * *
 * *  Return value: File* or NULL if failed
 * *
 * *************************************************************************/
FILE* transform_encrypt_fd(FILE* in_fd, unsigned char* key,
			   unsigned char** data) {
	unsigned char* buf = calloc(MAX_ENC_DATA, sizeof(unsigned char));
	if (buf == NULL) {
		write_log(10, "Failed to allocate memory in transform_encrypt_fd\n");
		return NULL;
	}
	int read_count = fread(buf, sizeof(unsigned char), MAX_ENC_DATA,
			       in_fd);
	unsigned char* new_data = calloc(read_count+TAG_SIZE,
					 sizeof(unsigned char));
	if (new_data == NULL) {
		free(buf);
		write_log(10, "Failed to allocate memory in transform_encrypt_fd\n");
		return NULL;
	}
	int ret = aes_gcm_encrypt_fix_iv(new_data, buf, read_count, key);
	if(ret != 0) {
		free(buf);
		write_log(10, "Failed encrypt. Code: %d\n", ret);
		return NULL;
	}
	free(buf);
	*data = new_data;
	return fmemopen(new_data, read_count+TAG_SIZE, "r");
}



/************************************************************************
 * *
 * * Function name: decrypt_to_fd
 * *        Inputs: FILE* decrypt_to_fd, open with 'w' mode
 *		    unsigned char* key
 *		    FILE* in_fd
 * *       Summary: Decrypt content in in_fd and write to decrypt_to_fd
 * *
 * *  Return value: 0 if success or 1 if failed
 * *
 * *************************************************************************/
int decrypt_to_fd(FILE* decrypt_to_fd, unsigned char* key, FILE* in_fd) {
	unsigned char* buf = calloc(MAX_ENC_DATA, sizeof(unsigned char));
	if (buf == NULL) {
		write_log(10, "Failed to allocate memory in decrypt_to_fd\n");
		return 1;
	}
	int read_count = fread(buf, sizeof(unsigned char), MAX_ENC_DATA,
			       in_fd);
	unsigned char* new_data = calloc(read_count-TAG_SIZE,
					 sizeof(unsigned char));
	if (new_data == NULL) {
		free(buf);
		write_log(10, "Failed to allocate memory in decrypt_to_fd\n");
		return 1;
	}
	int ret = aes_gcm_decrypt_fix_iv(new_data, buf, read_count, key);
	if(ret != 0) {
		free(buf);
		write_log(10, "Failed decrypt. Code: %d\n", ret);
		return 1;
	}
	free(buf);
	fwrite(new_data, sizeof(unsigned char), read_count-TAG_SIZE,
	       decrypt_to_fd);
	free(new_data);
	return 0;
}

/*
int main(void){
	int ret = 0;
	char* b64_input = "hello world!!\n";
	int b64_input_len = strlen(b64_input);
	printf("%d\n", b64_input_len);
	int tmp = expect_b64_encode_length(b64_input_len);
	int out_len = 0;
	char* b64_output = calloc(tmp, sizeof(char));
	b64encode_str((unsigned char*)b64_input, (unsigned char*)b64_output,
		      &out_len, b64_input_len);
	printf("%d %d\n", tmp, out_len);
	printf("%s\n", b64_output);
	char* b64_back = calloc(out_len, sizeof(char));
	ret = b64decode_str(b64_output, b64_back ,&out_len, strlen(b64_output));
	printf("%d\n", ret);
	b64_back = realloc(b64_back, out_len);
	printf("%s", b64_back);
	printf("%d\n", out_len);
	free(b64_output);
	free(b64_back);

	FILE* f = fopen("./enc.c", "r");
	unsigned char* key = get_key();
	printf("key: %d\n", key[31]);
	unsigned char* data = NULL;
	FILE* new_f = transform_encrypt_fd(f, key, &data);
	FILE* new_f_f = fopen("/tmp/test", "w");
	decrypt_to_fd(new_f_f, key, new_f);
	fclose(f);
	fclose(new_f);
	fclose(new_f_f);
	free(data);
	free(key);
}
*/
