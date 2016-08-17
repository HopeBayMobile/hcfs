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
#include "compress.h"
#include "b64encode.h"

/************************************************************************
 * *
 * * Function name: expect_b64_encode_length
 * *        Inputs: int32_t length
 * *       Summary: Calculate how many bytes will be after encode
 * *
 * *  Return value: bytes expected
 * *
 * *************************************************************************/
int32_t expect_b64_encode_length(uint32_t length)
{
	int32_t tmp = length % 3;

	tmp = (tmp == 0) ? tmp : (3 - tmp);
	/* 1 is for b64encode_str puts '\0' in the end */
	return 1 + (length + tmp) * 4 / 3;
}

/************************************************************************
 * *
 * * Function name: generate_random_bytes
 * *        Inputs: uint8_t* bytes: points to a buffer which
 *		    length should equals length
 *		    uint32_t length
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
int32_t generate_random_bytes(uint8_t *bytes, uint32_t length)
{
	if (length <= 0)
		return -1;

	memset(bytes, 0, length);
	int32_t rand_success = RAND_bytes(bytes, length);
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
 * *        Inputs: uint8_t* key: points to a buffer which
 *		    length should equals KEY_SIZE
 * *       Summary: generate a random key for encryption
 * *
 * *  Return value: See get_random_bytes
 * *
 * *************************************************************************/
int32_t generate_random_aes_key(uint8_t *key)
{
	return generate_random_bytes(key, KEY_SIZE);
}

/************************************************************************
 * *
 * * Function name: aes_gcm_encrypt_core
 * *        Inputs: uint8_t* output: points to a buffer which
 *		                           length should equals
 *		                           input_length + TAG_SIZE
 *		    uint8_t* input
 *		    uint32_t input_length
 *		    uint8_t* key: must be KEY_SIZE length
 *		    uint8_t* iv: must be IV_SIZE length
 * *       Summary: Use aes gcm mode to encrypt input
 * *
 * *  Return value: 0 if successful.
 *                  1 if Encrypt Update error
 *                  2 if Encrypt Final error
 *                  3 if extract TAG error
 * *
 * *************************************************************************/
int32_t aes_gcm_encrypt_core(uint8_t *output, uint8_t *input,
			 uint32_t input_length, uint8_t *key,
			 uint8_t *iv)
{
	int32_t tmp_length = 0;
	int32_t output_length = 0;
	int32_t retcode = 0;
	const int32_t output_preserve_size = TAG_SIZE;
	uint8_t tag[TAG_SIZE] = {0};
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
 * *        Inputs: uint8_t* output:  points to a buffer which
 *		    length should equals input_length - TAG_SIZE
 *		    uint8_t* input
 *		    uint32_t input_length
 *		    uint8_t* key: must KEY_SIZE length
 *		    uint8_t* iv: must IV_SIZE length
 * *       Summary: Use aes gcm mode to decrypt input
 * *
 * *  Return value: 0 if successful.
 *                  3 if set reference TAG error
 *                  1 if Decrypt update error
 *                  2 if Decrypr final error (TAG wrong)
 * *
 * *************************************************************************/
int32_t aes_gcm_decrypt_core(uint8_t *output, uint8_t *input,
			 uint32_t input_length, uint8_t *key,
			 uint8_t *iv)
{
	int32_t tmp_length = 0;
	int32_t output_length = 0;
	int32_t retcode = 0;
	uint8_t tag[TAG_SIZE] = {0};
	const int32_t preserve_size = TAG_SIZE;

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
 * *        Inputs: uint8_t* output: points to a buffer which
 *		    length should equals input_length + TAG_SIZE
 *		    uint8_t* input
 *		    uint32_t input_length
 *		    uint8_t* key
 * *       Summary: Use aes gcm mode to encrypt input (iv set to all zero)
 *		    It is dangerous if you use this function with a same key
 *		    again and again. So make sure you call this function
 *		    everytime with a different key.
 * *
 * *  Return value: See aes_gcm_encrypt_core
 * *
 * *************************************************************************/
int32_t aes_gcm_encrypt_fix_iv(uint8_t *output, uint8_t *input,
			   uint32_t input_length, uint8_t *key)
{
	uint8_t iv[IV_SIZE] = {0};

	return aes_gcm_encrypt_core(output, input, input_length, key, iv);
}

/************************************************************************
 * *
 * * Function name: aes_gcm_decrypt_fix_iv
 * *        Inputs: uint8_t* output: points to a buffer which
 *		    length should equals input_length - TAG_SIZE
 *		    uint8_t* input
 *		    uint32_t input_length
 *		    uint8_t* key
 * *       Summary: Use aes gcm mode to decrypt input (iv set to all zero)
 * *
 * *  Return value: See aes_gcm_decrypt_core
 * *
 * *************************************************************************/
int32_t aes_gcm_decrypt_fix_iv(uint8_t *output, uint8_t *input,
			   uint32_t input_length, uint8_t *key)
{
	uint8_t iv[IV_SIZE] = {0};

	return aes_gcm_decrypt_core(output, input, input_length, key, iv);
}

/*
 * This function only for developing "upload to cloud".
 * In the future, it should be reimplemented considering
 * key management specs
 */
uint8_t *get_key(const char *passphrase)
{
	const char *user_pass = passphrase;
	uint8_t md_value[EVP_MAX_MD_SIZE];
	uint32_t md_len;
	uint8_t *ret =
	    (uint8_t *)calloc(KEY_SIZE, sizeof(uint8_t));
	if (!ret)
		return NULL;
	const EVP_MD *m;
	EVP_MD_CTX ctx;

	m = EVP_sha256();

	if (!m) {
		free(ret);
		return NULL;
	}
	EVP_DigestInit(&ctx, m);
	uint8_t *salt = (uint8_t *)"oluik.354jhmnk,";

	PKCS5_PBKDF2_HMAC(user_pass, strlen(user_pass), salt,
			  strlen((char *)salt), 3, m, KEY_SIZE, ret);
	EVP_DigestFinal(&ctx, md_value, &md_len);
	return ret;
}

/************************************************************************
 * *
 * * Function name: transform_encrypt_fd
 * *        Inputs: FILE* in_fd, open with 'r' mode
 *		    uint8_t* key
 *		    uint8_t** data
 * *       Summary: Encrypt content read from in_fd, and return a new fd
 *		    data must be free outside this function
 * *
 * *  Return value: File* or NULL if failed
 * *
 * *************************************************************************/
FILE *transform_encrypt_fd(FILE *in_fd, uint8_t *key,
			   uint8_t **data)
{
#if COMPRESS_ENABLE
	uint8_t *buf =
	    calloc(compress_bound_f(MAX_BLOCK_SIZE), sizeof(uint8_t));
#else
	uint8_t *buf = calloc(MAX_BLOCK_SIZE, sizeof(uint8_t));
#endif
	if (buf == NULL) {
		write_log(
		    0, "Failed to allocate memory in transform_encrypt_fd\n");
		return NULL;
	}
#if COMPRESS_ENABLE
	int32_t read_count = fread(buf, sizeof(uint8_t),
			       compress_bound_f(MAX_BLOCK_SIZE), in_fd);
#else
	int32_t read_count = fread(buf, sizeof(uint8_t),
			       MAX_BLOCK_SIZE, in_fd);
#endif
	uint8_t *new_data =
	    calloc(read_count + TAG_SIZE, sizeof(uint8_t));
	if (new_data == NULL) {
		free(buf);
		write_log(
		    0, "Failed to allocate memory in transform_encrypt_fd\n");
		return NULL;
	}
	int32_t ret = aes_gcm_encrypt_fix_iv(new_data, buf, read_count, key);

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
	fwrite(new_data, sizeof(uint8_t), read_count + TAG_SIZE,
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
 *		    uint8_t* key
 *		    uint8_t* input
 *		    int32_t input_length
 * *       Summary: Decrypt write to decrypt_to_fd
 * *
 * *  Return value: 0 if success or 1 if failed
 * *
 * *************************************************************************/
int32_t decrypt_to_fd(FILE *decrypt_to_fd, uint8_t *key, uint8_t *input,
		  int32_t input_length)
{
	write_log(10, "decrypt_size: %d\n", input_length);
	uint8_t *output = (uint8_t *)calloc(input_length - TAG_SIZE,
							sizeof(uint8_t));
	int32_t ret = aes_gcm_decrypt_fix_iv(output, input, input_length, key);

	if (ret != 0) {
		free(output);
		write_log(2, "Failed decrypt. Code: %d\n", ret);
		return 1;
	}
	fwrite(output, sizeof(uint8_t), input_length - TAG_SIZE,
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
FILE *transform_fd(FILE *in_fd, uint8_t *key, uint8_t **data,
		   int32_t enc_flag, int32_t compress_flag)
{

	if (enc_flag && !compress_flag) {
		return transform_encrypt_fd(in_fd, key, data);
	}
	if (!enc_flag && compress_flag) {
		return transform_compress_fd(in_fd, data);
	}
	if (enc_flag && compress_flag) {
		uint8_t *compress_data;
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
 *                uint8_t* key, uint8_t* input,
 *                int32_t input_length, int32_t enc_flag, int32_t compress_flag
 *       Summary: Decode to to_fd
 *
 *  Return value: 0 if success or 1 if failed
 *
 *************************************************************************/
int32_t decode_to_fd(FILE *to_fd, uint8_t *key, uint8_t *input,
		 int32_t input_length, int32_t enc_flag, int32_t compress_flag)
{
	if (enc_flag && !compress_flag) {
		return decrypt_to_fd(to_fd, key, input, input_length);
	}
	if (!enc_flag && compress_flag) {
		return decompress_to_fd(to_fd, input, input_length);
	}
	if (enc_flag && compress_flag) {
		uint8_t *output = (uint8_t *)calloc(
		    input_length - TAG_SIZE, sizeof(uint8_t));
		int32_t ret =
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

	fwrite(input, sizeof(uint8_t), input_length, to_fd);
	return 0;
}

/************************************************************************
 * *
 * * Function name: get_decode_meta
 * *        Inputs: HCFS_encode_object_meta *, uint8_t *session_key
 * *                uint8_t *key, int32_t enc_flag, int_compress_flag
 * *       Summary: encrypt session_key and write to meta
 * *
 * *  Return value: 0 if success or -1 if failed
 * *
 * *************************************************************************/
int32_t get_decode_meta(HCFS_encode_object_meta *meta, uint8_t *session_key,
		    uint8_t *key, int32_t enc_flag, int32_t compress_flag)
{

	int32_t retCode = 0;
	int32_t ret = 0;

	meta->enc_alg = ENC_ALG_NONE;
	meta->len_enc_session_key = 0;
	meta->enc_session_key = NULL;

	if (compress_flag) {
		meta->comp_alg = compress_flag;
	} else {
		meta->comp_alg = COMP_ALG_NONE;
	}

	if (enc_flag) {
		int32_t outlen = 0;
		int32_t len_cipher = KEY_SIZE + IV_SIZE + TAG_SIZE;
		uint8_t buf[KEY_SIZE + IV_SIZE + TAG_SIZE] = {0};
		uint8_t iv[IV_SIZE] = {0};

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
		b64encode_str(buf, meta->enc_session_key, &outlen, len_cipher);
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
 * *        Inputs: uint8_t *session_key
 * *                uint8_t *enc_session_key, uint8_t *key
 * *       Summary: decrypt enc_seesion_key to session_key
 * *
 * *  Return value: 0 if success
 * *                -99 if invalid input
 * *                -101 if illegal character appears in b64decode
 * *                -102 if impossible format occurs in b64decode
 * *
 * *************************************************************************/
int32_t decrypt_session_key(uint8_t *session_key, char *enc_session_key,
			uint8_t *key)
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

	uint8_t buf[KEY_SIZE + IV_SIZE + TAG_SIZE] = {0};
	int32_t outlen = 0;
	int32_t ret = b64decode_str(enc_session_key, buf, &outlen,
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
*        Inputs: uint8_t *config_path
*       Summary: Helper function to read encrypted "config_path",
*                and write the decrypted contents to a temp file.
*  Return value: File pointer to decrypted config, or NULL if error
*                occured.
*
*************************************************************************/
FILE *get_decrypt_configfp(const char *config_path)
{

	int64_t file_size, enc_size, data_size;
	FILE *datafp = NULL;
	uint8_t *iv_buf = NULL;
	uint8_t *enc_buf = NULL;
	uint8_t *data_buf = NULL;
	uint8_t *enc_key = NULL;

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

	iv_buf = (uint8_t*)malloc(sizeof(char)*IV_SIZE);
	enc_buf = (uint8_t*)malloc(sizeof(char)*(enc_size));
	data_buf = (uint8_t*)malloc(sizeof(char)*(data_size));

	if (!iv_buf || !enc_buf || !data_buf)
		goto error;

	enc_key = get_key(CONFIG_PASSPHRASE);
	fread(iv_buf, sizeof(uint8_t), IV_SIZE, datafp);
	fread(enc_buf, sizeof(uint8_t), enc_size, datafp);

	if (aes_gcm_decrypt_core(data_buf, enc_buf, enc_size,
				 enc_key, iv_buf) != 0)
		goto error;

	FILE *tmp_file = tmpfile();
	if (tmp_file == NULL)
		goto error;
	fwrite(data_buf, sizeof(uint8_t), data_size,
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

/**
 * Encrypt json string and backup it.
 *
 * Given a valid json string, encrypt it and write to a specified file named
 * "usermeta", which is in the folder METAPATH.
 * 
 * @param json_str The json string to be enc.
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t enc_backup_usermeta(char *json_str)
{
	uint8_t *enc_key;
	uint8_t enc_data_tmp[strlen(json_str) + TAG_SIZE];
	uint8_t enc_data[strlen(json_str) + IV_SIZE + TAG_SIZE];
	uint8_t iv[IV_SIZE] = {0};
	int32_t len, ret, errcode;
	char path[200];
	FILE *fptr;
	size_t ret_size;

	write_log(10, "Debug: enc usermeta, json str is %s\n", json_str);
	len = strlen(json_str);
	enc_key = get_key(USERMETA_PASSPHRASE);
	generate_random_bytes(iv, IV_SIZE);

	ret = aes_gcm_encrypt_core(enc_data_tmp, (uint8_t *)json_str,
			len, enc_key, iv);
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
	FWRITE(enc_data, sizeof(uint8_t),
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

/**
 * Decrypt the content of given path and return the string, which
 * is a json string.
 *
 * @param path Path of the file to be decrypt
 *
 * @return 0 on success, otherwise negative error code.
 */
char *dec_backup_usermeta(char *path)
{
	int64_t enc_size, data_size;
	uint8_t *iv_buf = NULL;
	uint8_t *enc_buf = NULL;
	uint8_t *enc_key = NULL;
	char *data_buf = NULL; /* Data to be returned */
	FILE *fptr = NULL;
	int32_t ret, errcode;
	int64_t ret_pos;
	size_t ret_size;

	errcode = 0;
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
	iv_buf = (uint8_t*)malloc(sizeof(char)*IV_SIZE);
	enc_buf = (uint8_t*)malloc(sizeof(char)*(enc_size));
	data_buf = (char*)calloc(sizeof(char)*(data_size), 1);
	if (!iv_buf || !enc_buf || !data_buf) {
		write_log(0, "Error: Out of memory\n");
		errcode = -ENOMEM;
		goto errcode_handle;
	}

	enc_key = get_key(USERMETA_PASSPHRASE);
	FREAD(iv_buf, sizeof(uint8_t), IV_SIZE, fptr);
	FREAD(enc_buf, sizeof(uint8_t), (uint64_t)enc_size,
			fptr);

	ret = aes_gcm_decrypt_core((uint8_t *)data_buf, enc_buf,
			enc_size, enc_key, iv_buf);
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
