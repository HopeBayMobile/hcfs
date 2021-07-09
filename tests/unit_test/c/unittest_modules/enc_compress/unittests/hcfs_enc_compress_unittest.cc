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
#include <gtest/gtest.h>
#include <string.h>
#include <stdio.h>
#include <ftw.h>
extern "C" {
#include "params.h"
#include "b64encode.h"
#include "enc.h"
#ifndef _ANDROID_ENV_
#include "compress.h"
#endif
}

#define PASSPHRASE "this is hopebay testing"

extern SYSTEM_CONF_STRUCT *system_config;

static int do_delete (const char *fpath, const struct stat *sb,
		int32_t tflag, struct FTW *ftwbuf)
{
	switch (tflag) {
		case FTW_D:
		case FTW_DNR:
		case FTW_DP:
			rmdir (fpath);
			break;
		default:
			unlink (fpath);
			break;
	}
	return (0);
}

class enc : public testing::Test
{
      protected:
	char *input;
	int32_t input_size;
	virtual void SetUp()
	{
		system_config = (SYSTEM_CONF_STRUCT *)
			malloc(sizeof(SYSTEM_CONF_STRUCT));
		memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
		system_config->max_block_size = 1024;
		input =
		    (char *)calloc(sizeof(char), system_config->max_block_size);
		input_size = system_config->max_block_size;
		RAND_bytes((uint8_t *)input, input_size);
	}
	virtual void TearDown() { free(input); free(system_config); }
};

#ifndef _ANDROID_ENV_
class compress : public testing::Test
{
      protected:
	char *input;
	int32_t input_size;
	virtual void SetUp()
	{
		system_config = (SYSTEM_CONF_STRUCT *)
			malloc(sizeof(SYSTEM_CONF_STRUCT));
		memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
		system_config->max_block_size = 1024;
		input =
		    (char *)calloc(sizeof(char), system_config->max_block_size);
		input_size = system_config->max_block_size;
		RAND_bytes((uint8_t *)input, input_size);
	}
	virtual void TearDown() { free(input); free(system_config); }
};

TEST_F(compress, compress)
{
	int32_t output_max_size = compress_bound_f(input_size);
	char *compressed = (char *)calloc(output_max_size, sizeof(char));
	int32_t compress_size = compress_f(input, compressed, input_size);
	char *back = (char *)calloc(MAX_BLOCK_SIZE, sizeof(char));
	int32_t back_size =
	    decompress_f(compressed, back, compress_size, MAX_BLOCK_SIZE);
	EXPECT_TRUE(back_size == input_size);
	EXPECT_EQ(memcmp(input, back, input_size), 0);
	free(compressed);
	free(back);
}

TEST_F(compress, transform_compress_fd)
{

	FILE *in_file = fmemopen((void *)input, input_size, "r");
	uint8_t *data;
	FILE *new_compress_fd = transform_compress_fd(in_file, &data);
	EXPECT_TRUE(new_compress_fd != NULL);

	uint8_t *ptr = (uint8_t *)calloc(
	    compress_bound_f(input_size), sizeof(uint8_t));
	int32_t read_count = fread(ptr, sizeof(uint8_t),
			       compress_bound_f(input_size), new_compress_fd);
	char *ptr2 = NULL;
	size_t t = 0;
	FILE *in_fd = open_memstream(&ptr2, &t);
	decompress_to_fd(in_fd, ptr, read_count);

	fclose(in_fd);
	EXPECT_EQ(memcmp(ptr2, input, input_size), 0);

	fclose(new_compress_fd);
	fclose(in_file);
	free(data);
	free(ptr);
	free(ptr2);
}
#endif

TEST(base64, encode_then_decode)
{
	uint8_t *input = (uint8_t *)calloc(60, sizeof(uint8_t));
  generate_random_bytes((uint8_t*)input, 60);
	int32_t length_encode = expect_b64_encode_length(60);
	char *code = (char *)calloc(length_encode, sizeof(char));
	int32_t out_len = 0;
	b64encode_str(input, code, &out_len, 60);
	uint8_t *decode =
	    (uint8_t *)calloc(out_len + 1, sizeof(uint8_t));
	b64decode_str(code, decode, &out_len, out_len);
	EXPECT_EQ(memcmp(input, decode, 60), 0);
  free(input);
	free(code);
	free(decode);
}

TEST_F(enc, encrypt_with_fix_iv)
{
	uint8_t *key = get_key(PASSPHRASE);
	uint8_t iv[IV_SIZE] = {0};
	uint8_t *output1 = (uint8_t *)calloc(input_size + TAG_SIZE,
							 sizeof(uint8_t));
	uint8_t *output2 = (uint8_t *)calloc(input_size + TAG_SIZE,
							 sizeof(uint8_t));
	int32_t ret1 = aes_gcm_encrypt_core(output1, (uint8_t *)input,
					input_size, key, iv);
	int32_t ret2 = aes_gcm_encrypt_fix_iv(output2, (uint8_t *)input,
					  input_size, key);
	EXPECT_EQ(ret1, 0);
	EXPECT_EQ(ret2, 0);
	EXPECT_EQ(memcmp(output1, output2, input_size + TAG_SIZE), 0);
	free(output1);
	free(output2);
	free(key);
}

TEST_F(enc, encrypt_then_decrypt)
{
	uint8_t *key = get_key(PASSPHRASE);
	uint8_t *output = (uint8_t *)calloc(input_size + TAG_SIZE,
							sizeof(uint8_t));
	uint8_t *decode =
	    (uint8_t *)calloc(input_size, sizeof(uint8_t));

	int32_t ret = aes_gcm_encrypt_fix_iv(output, (uint8_t *)input,
					 input_size, key);
	EXPECT_EQ(ret, 0);
	ret =
	    aes_gcm_decrypt_fix_iv(decode, output, input_size + TAG_SIZE, key);
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(memcmp(input, decode, input_size), 0);
	free(key);
	free(output);
	free(decode);
}

TEST_F(enc, transform_encrypt_fd)
{
	FILE *in_file = fmemopen((void *)input, input_size, "r");
	uint8_t *key = get_key(PASSPHRASE);
	uint8_t *data;
	FILE *new_encrypt_fd = transform_encrypt_fd(in_file, key, &data);
	EXPECT_TRUE(new_encrypt_fd != NULL);

	uint8_t *ptr = (uint8_t *)calloc(input_size + TAG_SIZE,
						     sizeof(uint8_t));
	fread(ptr, sizeof(uint8_t), input_size + TAG_SIZE,
	      new_encrypt_fd);
	char *ptr2 = NULL;
	size_t t = 0;
	FILE *in_fd = open_memstream(&ptr2, &t);
	decrypt_to_fd(in_fd, key, ptr, input_size + TAG_SIZE);

	fclose(in_fd);
	EXPECT_EQ(memcmp(ptr2, input, input_size), 0);

	fclose(new_encrypt_fd);
	fclose(in_file);
	free(key);
	free(data);
	free(ptr);
	free(ptr2);
}

TEST_F(enc, transform_fd_no_flag)
{
	uint8_t *data;
	FILE *in_file = fmemopen((void *)input, input_size, "r");
	FILE *new_file = transform_fd(in_file, NULL, &data, 0, 0);
	EXPECT_TRUE(new_file != NULL);
	EXPECT_TRUE(new_file == in_file);

	uint8_t *ptr =
	    (uint8_t *)calloc(input_size, sizeof(uint8_t));
	int32_t read_count =
	    fread(ptr, sizeof(uint8_t), input_size, new_file);
	char *ptr2 = NULL;
	size_t t = 0;
	FILE *in_fd = open_memstream(&ptr2, &t);
	decode_to_fd(in_fd, NULL, ptr, read_count, 0, 0);

	fclose(in_fd);
	EXPECT_EQ(memcmp(ptr2, input, input_size), 0);

	fclose(in_file);
	if (new_file != in_file) {
		fclose(new_file);
	}
	free(ptr);
	free(ptr2);
}

TEST_F(enc, transform_fd_enc_flag)
{
	FILE *in_file = fmemopen((void *)input, input_size, "r");
	uint8_t *key = get_key(PASSPHRASE);
	uint8_t *data;
	FILE *new_encrypt_fd = transform_fd(in_file, key, &data, 1, 0);
	EXPECT_TRUE(new_encrypt_fd != NULL);

	uint8_t *ptr = (uint8_t *)calloc(input_size + TAG_SIZE,
						     sizeof(uint8_t));
	fread(ptr, sizeof(uint8_t), input_size + TAG_SIZE,
	      new_encrypt_fd);
	char *ptr2 = NULL;
	size_t t = 0;
	FILE *in_fd = open_memstream(&ptr2, &t);
	decode_to_fd(in_fd, key, ptr, input_size + TAG_SIZE, 1, 0);

	fclose(in_fd);
	EXPECT_EQ(memcmp(ptr2, input, input_size), 0);

	fclose(new_encrypt_fd);
	fclose(in_file);
	free(key);
	free(data);
	free(ptr);
	free(ptr2);
}

#ifndef _ANDROID_ENV_
TEST_F(enc, transform_fd_compress_flag)
{
	FILE *in_file = fmemopen((void *)input, input_size, "r");
	uint8_t *data;
	FILE *new_compress_fd = transform_fd(in_file, NULL, &data, 0, 1);
	EXPECT_TRUE(new_compress_fd != NULL);

	uint8_t *ptr = (uint8_t *)calloc(
	    compress_bound_f(input_size), sizeof(uint8_t));
	int32_t read_count = fread(ptr, sizeof(uint8_t),
			       compress_bound_f(input_size), new_compress_fd);
	char *ptr2 = NULL;
	size_t t = 0;
	FILE *in_fd = open_memstream(&ptr2, &t);
	decode_to_fd(in_fd, NULL, ptr, read_count, 0, 1);

	fclose(in_fd);
	EXPECT_EQ(memcmp(ptr2, input, strlen(input)), 0);

	fclose(new_compress_fd);
	fclose(in_file);
	free(data);
	free(ptr);
	free(ptr2);
}

TEST_F(enc, transform_fd_both_flag)
{
	FILE *in_file = fmemopen((void *)input, input_size, "r");
	uint8_t *key = get_key(PASSPHRASE);
	uint8_t *data;
	FILE *new_encrypt_fd = transform_fd(in_file, key, &data, 1, 1);
	EXPECT_TRUE(new_encrypt_fd != NULL);

	uint8_t *ptr = (uint8_t *)calloc(
	    compress_bound_f(input_size) + TAG_SIZE, sizeof(uint8_t));
	int32_t read_count =
	    fread(ptr, sizeof(uint8_t),
		  compress_bound_f(input_size) + TAG_SIZE, new_encrypt_fd);
	char *ptr2 = NULL;
	size_t t = 0;
	FILE *in_fd = open_memstream(&ptr2, &t);
	int32_t ret = decode_to_fd(in_fd, key, ptr, read_count, 1, 1);
	EXPECT_EQ(ret, 0);

	fclose(in_fd);
	EXPECT_EQ(memcmp(ptr2, input, input_size), 0);

	fclose(new_encrypt_fd);
	fclose(in_file);
	free(key);
	free(data);
	free(ptr);
	free(ptr2);
}
#endif

TEST_F(enc, get_decode_meta){
  HCFS_encode_object_meta object_meta;
  uint8_t *session_key = (uint8_t *)calloc(KEY_SIZE, sizeof(uint8_t));
  //uint8_t *iv = (uint8_t *)calloc(IV_SIZE, sizeof(uint8_t));
  uint8_t *key = get_key(PASSPHRASE);

  int32_t ret = get_decode_meta(&object_meta, session_key, key, 1, 1);
  EXPECT_EQ(ret, 0);

  printf("%s\n", object_meta.enc_session_key);

  uint8_t *back_session_key = (uint8_t *)calloc(KEY_SIZE, sizeof(uint8_t));
  ret = decrypt_session_key(back_session_key, object_meta.enc_session_key, key);
  EXPECT_EQ(ret, 0);
  //EXPECT_TRUE(memcmp(session_key, back_session_key, KEY_SIZE) == 0);

  if(object_meta.enc_session_key)
    free(object_meta.enc_session_key);
  OPENSSL_free(key);
  OPENSSL_free(session_key);
  OPENSSL_free(back_session_key);
}

TEST(get_decrypt_configfpTEST, config_path_not_found)
{
	const char path[100] = "/path/not/existed";
	FILE* ret_fp = get_decrypt_configfp(path);

	int32_t ret = (ret_fp) ? 0 : -1;

	EXPECT_EQ(ret, -1);
}

TEST(get_decrypt_configfpTEST, config_content_error)
{
	const char path[100] = "testpatterns/not_encrypted.conf";
	FILE* ret_fp = get_decrypt_configfp(path);

	int32_t ret = (ret_fp) ? 0 : -1;

	EXPECT_EQ(ret, -1);
}

TEST(get_decrypt_configfpTEST, getOK)
{
	char buf[200], buf2[200];
	char unenc_path[100] = "testpatterns/not_encrypted.conf";
	const char enc_path[100] = "testpatterns/encrypted.conf";
	FILE *unenc_fp = NULL;
	FILE *enc_fp = NULL;

	enc_fp = get_decrypt_configfp(enc_path);
	int32_t ret = (enc_fp) ? 0 : -1;
	EXPECT_EQ(ret, 0);

	unenc_fp = fopen(unenc_path, "r");

	ret = 0;
	while (fgets(buf, sizeof(buf), unenc_fp) != NULL) {
		if (fgets(buf2, sizeof(buf2), enc_fp) == NULL ||
			strcmp(buf, buf2) != 0) {
			ret = -1;
			break;
		}
	}

	if (fgets(buf2, sizeof(buf2), enc_fp) != NULL)
		ret = -1;

	EXPECT_EQ(ret, 0);
}

class enc_backup_usermetaTest : public testing::Test {
protected:
	char usermeta_path[200];
	char *ret_str;
	void SetUp()
	{
		system_config = (SYSTEM_CONF_STRUCT *)
			malloc(sizeof(SYSTEM_CONF_STRUCT));
		memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
		METAPATH = "/tmp/backup_usermeta_test";
		sprintf(usermeta_path, "%s/usermeta", METAPATH);
		if (!access(usermeta_path, F_OK))
			unlink(usermeta_path);
		if (!access(METAPATH, F_OK))
			nftw(METAPATH, do_delete, 20, FTW_DEPTH);
		mkdir(METAPATH, 0700);
		ret_str = NULL;
	}

	void TearDown()
	{
		if (!access(usermeta_path, F_OK))
			unlink(usermeta_path);
		if (!access(METAPATH, F_OK))
			nftw(METAPATH, do_delete, 20, FTW_DEPTH);

		free(system_config);
	}
};

TEST_F(enc_backup_usermetaTest, CreateUsermeta)
{
	EXPECT_EQ(0, enc_backup_usermeta("alohaaloha"));
	EXPECT_EQ(0, access(usermeta_path, F_OK));

	ret_str = dec_backup_usermeta(usermeta_path);
	ASSERT_NE(0, (ret_str != NULL));
	EXPECT_EQ(0, strncmp("alohaaloha", ret_str, sizeof(ret_str))) << ret_str;

	free(ret_str);
	unlink(usermeta_path);
}

TEST_F(enc_backup_usermetaTest, ReCreateNewUsermeta)
{
	mknod(usermeta_path, 0700, 0);		

	EXPECT_EQ(0, enc_backup_usermeta("alohaaloha"));
	EXPECT_EQ(0, access(usermeta_path, F_OK));

	ret_str = dec_backup_usermeta(usermeta_path);
	ASSERT_NE((char *)NULL, ret_str);
	EXPECT_EQ(0, strncmp("alohaaloha", ret_str, sizeof(ret_str))) << ret_str;

	free(ret_str);
	unlink(usermeta_path);
}
