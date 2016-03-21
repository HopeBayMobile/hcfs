/*************************************************************************
 * *
 * * Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
 * *
 * * File Name: enc.c
 * * Abstract: The c source code file for some encryption helpers.
 * *
 * **************************************************************************/

#include "enc.h"

#include <sys/file.h>

#include "utils.h"
#include "macro.h"

/************************************************************************
 * *
 * * Function name: expect_b64_encode_length
 * *        Inputs: int length
 * *       Summary: Calculate how many bytes will be after encode
 * *
 * *  Return value: bytes expected
 * *
 * *************************************************************************/
int expect_b64_encode_length(unsigned int length)
{
	int tmp = length % 3;

	tmp = (tmp == 0) ? tmp : (3 - tmp);
	/* 1 is for b64encode_str puts '\0' in the end */
	return 1 + (length + tmp) * 4 / 3;
}

/************************************************************************
 * *
 * * Function name: generate_random_bytes
 * *        Inputs: unsigned char* bytes: points to a buffer which
 *		    length should equals length
 *		    unsigned int length
 * *       Summary: generate some random bytes
 * *
 * *
 * *  Return value: 0 if successful.
 *                  -1 if length <= 0.
 *                  -2 if openssl RAND not supported
 *                  -3 if some openssl error occurs, use ERR_get_error to get
 *                  error code
 * *
 * *************************************************************************/
int generate_random_bytes(unsigned char *bytes, unsigned int length)
{
	if (length <= 0)
		return -1;

	memset(bytes, 0, length);
	int rand_success = RAND_bytes(bytes, length);
	/* RAND_bytes() returns 1 on success, 0 otherwise. The error code can
	 * be obtained by ERR_get_error.
	 * return -1 if not supported by the current RAND method.
	 * https://wiki.openssl.org/index.php/Manual:RAND_bytes%283%29
	 */
	switch (rand_success) {
	case 1:
		return 0;
	case -1:
		write_log(0, "RAND_bytes not supported");
		return -2;
	default:
		write_log(1, "RAND_bytes: some openssl error may occurs: %d",
			  ERR_peek_last_error());
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
int generate_random_aes_key(unsigned char *key)
{
	return generate_random_bytes(key, KEY_SIZE);
}

/************************************************************************
 * *
 * * Function name: aes_gcm_encrypt_core
 * *        Inputs: unsigned char* output: points to a buffer which
 *		                           length should equals
 *		                           input_length + TAG_SIZE
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
int aes_gcm_encrypt_core(unsigned char *output, unsigned char *input,
			 unsigned int input_length, unsigned char *key,
			 unsigned char *iv)
{
	int tmp_length = 0;
	int output_length = 0;
	int retcode = 0;
	const int output_preserve_size = TAG_SIZE;
	unsigned char tag[TAG_SIZE] = {0};
	/* clear output */
	memset(output, 0, input_length + TAG_SIZE);
	EVP_CIPHER_CTX ctx;

	EVP_CIPHER_CTX_init(&ctx);
	EVP_EncryptInit_ex(&ctx, EVP_aes_256_gcm(), NULL, key, iv);
	if (!EVP_EncryptUpdate(&ctx, output + output_preserve_size, &tmp_length,
			       input, input_length)) {
		retcode = 1;
		goto final;
	}
	if (!EVP_EncryptFinal_ex(&ctx, output + output_preserve_size + tmp_length,
			      &output_length)) {
		retcode = 2;
		goto final;
	}
	if (!EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag)) {
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
int aes_gcm_decrypt_core(unsigned char *output, unsigned char *input,
			 unsigned int input_length, unsigned char *key,
			 unsigned char *iv)
{
	int tmp_length = 0;
	int output_length = 0;
	int retcode = 0;
	unsigned char tag[TAG_SIZE] = {0};
	const int preserve_size = TAG_SIZE;

	/* clear output */
	memset(output, 0, input_length - TAG_SIZE);

	EVP_CIPHER_CTX ctx;

	EVP_CIPHER_CTX_init(&ctx);
	EVP_DecryptInit_ex(&ctx, EVP_aes_256_gcm(), NULL, key, iv);
	if (!EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_SET_TAG, TAG_SIZE, input)) {
		retcode = 3;
		goto decrypt_final;
	}
	if (!EVP_DecryptUpdate(&ctx, output, &tmp_length, input + preserve_size,
			       input_length - TAG_SIZE)) {
		retcode = 1;
		goto decrypt_final;
	}
	if (!EVP_DecryptFinal_ex(&ctx, tag, &output_length)) {
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
int aes_gcm_encrypt_fix_iv(unsigned char *output, unsigned char *input,
			   unsigned int input_length, unsigned char *key)
{
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
int aes_gcm_decrypt_fix_iv(unsigned char *output, unsigned char *input,
			   unsigned int input_length, unsigned char *key)
{
	unsigned char iv[IV_SIZE] = {0};

	return aes_gcm_decrypt_core(output, input, input_length, key, iv);
}

/*
 * This function only for developing "upload to cloud".
 * In the future, it should be reimplemented considering
 * key management specs
 */
unsigned char *get_key(const char *passphrase)
{
	const char *user_pass = passphrase;
	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;
	unsigned char *ret =
	    (unsigned char *)calloc(KEY_SIZE, sizeof(unsigned char));
	if (!ret)
		return NULL;
	const EVP_MD *m;
	EVP_MD_CTX ctx;

	m = EVP_sha256();

	if (!m)
		return NULL;
	EVP_DigestInit(&ctx, m);
	unsigned char *salt = (unsigned char *)"oluik.354jhmnk,";

	PKCS5_PBKDF2_HMAC(user_pass, strlen(user_pass), salt,
			  strlen((char *)salt), 3, m, KEY_SIZE, ret);
	EVP_DigestFinal(&ctx, md_value, &md_len);
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
FILE *transform_encrypt_fd(FILE *in_fd, unsigned char *key,
			   unsigned char **data)
{
#if COMPRESS_ENABLE
	unsigned char *buf =
	    calloc(compress_bound_f(MAX_BLOCK_SIZE), sizeof(unsigned char));
#else
	unsigned char *buf = calloc(MAX_BLOCK_SIZE, sizeof(unsigned char));
#endif
	if (buf == NULL) {
		write_log(
		    0, "Failed to allocate memory in transform_encrypt_fd\n");
		return NULL;
	}
#if COMPRESS_ENABLE
	int read_count = fread(buf, sizeof(unsigned char),
			       compress_bound_f(MAX_BLOCK_SIZE), in_fd);
#else
	int read_count = fread(buf, sizeof(unsigned char),
			       MAX_BLOCK_SIZE, in_fd);
#endif
	unsigned char *new_data =
	    calloc(read_count + TAG_SIZE, sizeof(unsigned char));
	if (new_data == NULL) {
		free(buf);
		write_log(
		    0, "Failed to allocate memory in transform_encrypt_fd\n");
		return NULL;
	}
	int ret = aes_gcm_encrypt_fix_iv(new_data, buf, read_count, key);

	if (ret != 0) {
		free(buf);
		write_log(1, "Failed encrypt. Code: %d\n", ret);
		return NULL;
	}
	free(buf);
	*data = new_data;
	write_log(10, "encrypt_size: %d\n", read_count+TAG_SIZE);
#if defined(__ANDROID__) || defined(_ANDROID_ENV_)
	FILE *tmp_file = tmpfile();
	if (tmp_file == NULL) {
		write_log(2, "tmpfile() failed to create tmpfile\n");
		return NULL;
	}
	fwrite(new_data, sizeof(unsigned char), read_count + TAG_SIZE,
	       tmp_file);
	rewind(tmp_file);
	return tmp_file;
#else
	return fmemopen(new_data, read_count + TAG_SIZE, "rb");
#endif
}

/************************************************************************
 * *
 * * Function name: decrypt_to_fd
 * *        Inputs: FILE* decrypt_to_fd, open with 'w' mode
 *		    unsigned char* key
 *		    unsigned char* input
 *		    int input_length
 * *       Summary: Decrypt write to decrypt_to_fd
 * *
 * *  Return value: 0 if success or 1 if failed
 * *
 * *************************************************************************/
int decrypt_to_fd(FILE *decrypt_to_fd, unsigned char *key, unsigned char *input,
		  int input_length)
{
	write_log(10, "decrypt_size: %d\n", input_length);
	unsigned char *output = (unsigned char *)calloc(input_length - TAG_SIZE,
							sizeof(unsigned char));
	int ret = aes_gcm_decrypt_fix_iv(output, input, input_length, key);

	if (ret != 0) {
		free(output);
		write_log(2, "Failed decrypt. Code: %d\n", ret);
		return 1;
	}
	fwrite(output, sizeof(unsigned char), input_length - TAG_SIZE,
	       decrypt_to_fd);
	free(output);
	return 0;
}

/************************************************************************
 * *
 * * Function name: transform_fd
 * *        Inputs: FILE* in_fd, open with 'r' mode, key, data, enc_flag,
 * compress_flag
 * *       Summary: Combine transform_encrypt_fd and transform_compress_fd
 * functions
 * *  Return value: File* or NULL if failed
 * *
 * *************************************************************************/
FILE *transform_fd(FILE *in_fd, unsigned char *key, unsigned char **data,
		   int enc_flag, int compress_flag)
{

	if (enc_flag && !compress_flag) {
		return transform_encrypt_fd(in_fd, key, data);
	}
	if (!enc_flag && compress_flag) {
		return transform_compress_fd(in_fd, data);
	}
	if (enc_flag && compress_flag) {
		unsigned char *compress_data;
		FILE *compress_fd =
		    transform_compress_fd(in_fd, &compress_data);
		if (compress_fd == NULL)
			return NULL;
		FILE *ret = transform_encrypt_fd(compress_fd, key, data);

		fclose(compress_fd);
		free(compress_data);
		return ret;
	}
	return in_fd;
}

/************************************************************************
 *
 * Function name: decode_to_fd
 *        Inputs: FILE* to_fd, open with 'w' mode,
 *                unsigned char* key, unsigned char* input,
 *                int input_length, int enc_flag, int compress_flag
 *       Summary: Decode to to_fd
 *
 *  Return value: 0 if success or 1 if failed
 *
 *************************************************************************/
int decode_to_fd(FILE *to_fd, unsigned char *key, unsigned char *input,
		 int input_length, int enc_flag, int compress_flag)
{
	if (enc_flag && !compress_flag) {
		return decrypt_to_fd(to_fd, key, input, input_length);
	}
	if (!enc_flag && compress_flag) {
		return decompress_to_fd(to_fd, input, input_length);
	}
	if (enc_flag && compress_flag) {
		unsigned char *output = (unsigned char *)calloc(
		    input_length - TAG_SIZE, sizeof(unsigned char));
		int ret =
		    aes_gcm_decrypt_fix_iv(output, input, input_length, key);
		if (ret != 0) {
			free(output);
			write_log(2, "Failed decrypt. Code: %d\n", ret);
			return 1;
		}
		ret = decompress_to_fd(to_fd, output, input_length - TAG_SIZE);
		free(output);
		return ret;
	}

	fwrite(input, sizeof(unsigned char), input_length, to_fd);
	return 0;
}

/************************************************************************
 * *
 * * Function name: get_decode_meta
 * *        Inputs: HCFS_encode_object_meta *, unsigned char *session_key
 * *                unsigned char *key, int enc_flag, int_compress_flag
 * *       Summary: encrypt session_key and write to meta
 * *
 * *  Return value: 0 if success or -1 if failed
 * *
 * *************************************************************************/
int get_decode_meta(HCFS_encode_object_meta *meta, unsigned char *session_key,
		    unsigned char *key, int enc_flag, int compress_flag)
{

	int retCode = 0;
	int ret = 0;

	meta->enc_alg = ENC_ALG_NONE;
	meta->len_enc_session_key = 0;
	meta->enc_session_key = NULL;

	if (compress_flag) {
		meta->comp_alg = compress_flag;
	} else {
		meta->comp_alg = COMP_ALG_NONE;
	}

	if (enc_flag) {
		int outlen = 0;
		int len_cipher = KEY_SIZE + IV_SIZE + TAG_SIZE;
		unsigned char buf[KEY_SIZE + IV_SIZE + TAG_SIZE] = {0};
		unsigned char iv[IV_SIZE] = {0};

		meta->enc_alg = enc_flag;
		if (generate_random_aes_key(session_key) != 0)
			goto error;
		if (generate_random_bytes(iv, IV_SIZE) != 0)
			goto error;
		ret = aes_gcm_encrypt_core(buf + IV_SIZE, session_key, KEY_SIZE,
					   key, iv);
		if (ret != 0)
			goto error;
		memcpy(buf, iv, IV_SIZE);
		meta->len_enc_session_key =
		    expect_b64_encode_length(len_cipher);
		meta->enc_session_key =
		    calloc(meta->len_enc_session_key, sizeof(char));
		b64encode_str(buf, (unsigned char*)meta->enc_session_key, &outlen, len_cipher);
	}
	goto end;

error:
	retCode = -1;

end:
	return retCode;
}

/************************************************************************
 * *
 * * Function name: decrypt_session_key
 * *        Inputs: unsigned char *session_key
 * *                unsigned char *enc_session_key, unsigned char *key
 * *       Summary: decrypt enc_seesion_key to session_key
 * *
 * *  Return value: 0 if success
 * *                -99 if invalid input
 * *                -101 if illegal character appears in b64decode
 * *                -102 if impossible format occurs in b64decode
 * *
 * *************************************************************************/
int decrypt_session_key(unsigned char *session_key, char *enc_session_key,
			unsigned char *key)
{
	if (!session_key) {
		write_log(3, "session_key is NULL\n");
		return -99;
	}
	if (!enc_session_key) {
		write_log(3, "enc_session_key is NULL\n");
		return -99;
	}
	if (!key) {
		write_log(3, "key is NULL\n");
		return -99;
	}

	unsigned char buf[KEY_SIZE + IV_SIZE + TAG_SIZE] = {0};
	int outlen = 0;
	int ret = b64decode_str(enc_session_key, buf, &outlen,
				strlen(enc_session_key));
	if (ret != 0) {
		return ret - 100;
		/* -101 if illegal character occurs */
		/* -102 if impossible format occurs */
	}
	ret = aes_gcm_decrypt_core(session_key, buf + IV_SIZE,
				   KEY_SIZE + TAG_SIZE, key, buf);
	return ret;
}

void free_object_meta(HCFS_encode_object_meta *object_meta)
{
	if (object_meta != NULL) {
		if (object_meta->enc_session_key != NULL) {
			OPENSSL_free(object_meta->enc_session_key);
		}
		free(object_meta);
	}
}

/************************************************************************
*
* Function name: get_decrypt_configfp
*        Inputs: unsigned char *config_path
*       Summary: Helper function to read encrypted "config_path",
*                and write the decrypted contents to a temp file.
*  Return value: File pointer to decrypted config, or NULL if error
*                occured.
*
*************************************************************************/
FILE *get_decrypt_configfp(const char *config_path)
{

	long file_size, enc_size, data_size;
	FILE *datafp = NULL;
	unsigned char *iv_buf = NULL;
	unsigned char *enc_buf = NULL;
	unsigned char *data_buf = NULL;
	unsigned char *enc_key = NULL;

	if (access(config_path, F_OK | R_OK) == -1)
		goto error;

	datafp = fopen(config_path, "r");
	if (datafp == NULL)
		goto error;

	fseek(datafp, 0, SEEK_END);
	file_size = ftell(datafp);
	rewind(datafp);

	enc_size = file_size - IV_SIZE;
	data_size = enc_size - TAG_SIZE;

	iv_buf = (unsigned char*)malloc(sizeof(char)*IV_SIZE);
	enc_buf = (unsigned char*)malloc(sizeof(char)*(enc_size));
	data_buf = (unsigned char*)malloc(sizeof(char)*(data_size));

	if (!iv_buf || !enc_buf || !data_buf)
		goto error;

	enc_key = get_key(CONFIG_PASSPHRASE);
	fread(iv_buf, sizeof(unsigned char), IV_SIZE, datafp);
	fread(enc_buf, sizeof(unsigned char), enc_size, datafp);

	if (aes_gcm_decrypt_core(data_buf, enc_buf, enc_size,
				 enc_key, iv_buf) != 0)
		goto error;

	FILE *tmp_file = tmpfile();
	if (tmp_file == NULL)
		goto error;
	fwrite(data_buf, sizeof(unsigned char), data_size,
		tmp_file);

	rewind(tmp_file);
	goto end;

error:
	tmp_file = NULL;
end:
	if (datafp)
		fclose(datafp);
	if (enc_key)
		free(enc_key);
	if (data_buf)
		free(data_buf);
	if (enc_buf)
		free(enc_buf);
	if (iv_buf)
		free(iv_buf);

	return tmp_file;
}

int enc_backup_usermeta(char *json_str)
{
	unsigned char *enc_key;
	unsigned char enc_data_tmp[strlen(json_str) + TAG_SIZE];
	unsigned char enc_data[strlen(json_str) + IV_SIZE + TAG_SIZE];
	unsigned char iv[IV_SIZE] = {0};
	int len, ret, errcode;
	char path[200];
	FILE *fptr;
	size_t ret_size;

	write_log(10, "Debug: enc usermeta, json str is %s\n", json_str);
	len = strlen(json_str);
	enc_key = get_key("Enc the usermeta");
	generate_random_bytes(iv, IV_SIZE);

	ret = aes_gcm_encrypt_core(enc_data_tmp, json_str, len, enc_key, iv);
	if (ret != 0) {
	        return -1;
	}
	memcpy(enc_data, iv, IV_SIZE);
	memcpy(enc_data + IV_SIZE, enc_data_tmp, len + TAG_SIZE);

	/* Unlink old backup and enforce to create new one */
	sprintf(path, "%s/usermeta", METAPATH);
	if (access(path, F_OK) == 0)
		unlink(path);

	fptr = fopen(path, "w+");
	if (!fptr) {
		errcode = errno;
		return -errcode;
	}
	setbuf(fptr, NULL);
	flock(fileno(fptr), LOCK_EX);
	FSEEK(fptr, 0, SEEK_SET);
	FWRITE(enc_data, sizeof(unsigned char),
			len + IV_SIZE + TAG_SIZE, fptr);
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	write_log(10, "Debug: Finish enc usermeta\n");
	return 0;

errcode_handle:
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	return errcode;
}

char *dec_backup_usermeta(char *path)
{
	long long enc_size, data_size;
	unsigned char *iv_buf = NULL;
	unsigned char *enc_buf = NULL;
	unsigned char *enc_key = NULL;
	char *data_buf = NULL;
	FILE *fptr;
	int ret, errcode;
	long long ret_pos;
	size_t ret_size;

	if (access(path, F_OK) < 0)
		return NULL;

	fptr = fopen(path, "r");
	if (!fptr)
		return NULL;

	flock(fileno(fptr), LOCK_EX);
	FSEEK(fptr, 0, SEEK_END);
	FTELL(fptr);
	rewind(fptr);

	enc_size = ret_pos - IV_SIZE;
	data_size = enc_size - TAG_SIZE;
	iv_buf = (char*)malloc(sizeof(char)*IV_SIZE);
	enc_buf = (char*)malloc(sizeof(char)*(enc_size));
	data_buf = (char*)malloc(sizeof(char)*(data_size));
	if (!iv_buf || !enc_buf || !data_buf) {
		write_log(0, "Error: Out of memory\n");
		errcode = -ENOMEM;
		goto errcode_handle;
	}

	enc_key = get_key("Enc the usermeta");
	FREAD(iv_buf, sizeof(unsigned char), IV_SIZE, fptr);
	FREAD(enc_buf, sizeof(unsigned char), enc_size, fptr);

	ret = aes_gcm_decrypt_core(data_buf, enc_buf, enc_size,
				 enc_key, iv_buf);
	if (ret < 0) {
		write_log(0, "Fail to dec usermeta. Perhaps it is corrupt\n");
		errcode = ret;
		goto errcode_handle;
	}

errcode_handle:
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	if (enc_key)
		free(enc_key);
	if (enc_buf)
		free(enc_buf);
	if (iv_buf)
		free(iv_buf);
	if (errcode < 0) {
		if (data_buf)
			free(data_buf);
		return NULL;
	} else {
		return data_buf;
	}
}
