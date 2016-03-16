#include <gtest/gtest.h>
#include <string.h>
#include <stdio.h>
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

class enc : public testing::Test
{
      protected:
	char *input;
	int input_size;
	virtual void SetUp()
	{
		system_config = (SYSTEM_CONF_STRUCT *)
			malloc(sizeof(SYSTEM_CONF_STRUCT));
		memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
		system_config->max_block_size = 1024;
		input =
		    (char *)calloc(sizeof(char), system_config->max_block_size);
		input_size = system_config->max_block_size;
		RAND_bytes((unsigned char *)input, input_size);
	}
	virtual void TearDown() { free(input); free(system_config); }
};

#ifndef _ANDROID_ENV_
class compress : public testing::Test
{
      protected:
	char *input;
	int input_size;
	virtual void SetUp()
	{
		system_config = (SYSTEM_CONF_STRUCT *)
			malloc(sizeof(SYSTEM_CONF_STRUCT));
		memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
		system_config->max_block_size = 1024;
		input =
		    (char *)calloc(sizeof(char), system_config->max_block_size);
		input_size = system_config->max_block_size;
		RAND_bytes((unsigned char *)input, input_size);
	}
	virtual void TearDown() { free(input); free(system_config); }
};

TEST_F(compress, compress)
{
	int output_max_size = compress_bound_f(input_size);
	char *compressed = (char *)calloc(output_max_size, sizeof(char));
	int compress_size = compress_f(input, compressed, input_size);
	char *back = (char *)calloc(MAX_BLOCK_SIZE, sizeof(char));
	int back_size =
	    decompress_f(compressed, back, compress_size, MAX_BLOCK_SIZE);
	EXPECT_TRUE(back_size == input_size);
	EXPECT_EQ(memcmp(input, back, input_size), 0);
	free(compressed);
	free(back);
}

TEST_F(compress, transform_compress_fd)
{

	FILE *in_file = fmemopen((void *)input, input_size, "r");
	unsigned char *data;
	FILE *new_compress_fd = transform_compress_fd(in_file, &data);
	EXPECT_TRUE(new_compress_fd != NULL);

	unsigned char *ptr = (unsigned char *)calloc(
	    compress_bound_f(input_size), sizeof(unsigned char));
	int read_count = fread(ptr, sizeof(unsigned char),
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
  generate_random_bytes((unsigned char*)input, 60);
	int length_encode = expect_b64_encode_length(60);
	char *code = (char *)calloc(length_encode, sizeof(char));
	int out_len = 0;
	b64encode_str(input, code, &out_len, 60);
	unsigned char *decode =
	    (unsigned char *)calloc(out_len + 1, sizeof(unsigned char));
	b64decode_str(code, decode, &out_len, out_len);
	EXPECT_EQ(memcmp(input, decode, 60), 0);
  free(input);
	free(code);
	free(decode);
}

TEST_F(enc, encrypt_with_fix_iv)
{
	unsigned char *key = get_key(PASSPHRASE);
	unsigned char iv[IV_SIZE] = {0};
	unsigned char *output1 = (unsigned char *)calloc(input_size + TAG_SIZE,
							 sizeof(unsigned char));
	unsigned char *output2 = (unsigned char *)calloc(input_size + TAG_SIZE,
							 sizeof(unsigned char));
	int ret1 = aes_gcm_encrypt_core(output1, (unsigned char *)input,
					input_size, key, iv);
	int ret2 = aes_gcm_encrypt_fix_iv(output2, (unsigned char *)input,
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
	unsigned char *key = get_key(PASSPHRASE);
	unsigned char *output = (unsigned char *)calloc(input_size + TAG_SIZE,
							sizeof(unsigned char));
	unsigned char *decode =
	    (unsigned char *)calloc(input_size, sizeof(unsigned char));

	int ret = aes_gcm_encrypt_fix_iv(output, (unsigned char *)input,
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
	unsigned char *key = get_key(PASSPHRASE);
	unsigned char *data;
	FILE *new_encrypt_fd = transform_encrypt_fd(in_file, key, &data);
	EXPECT_TRUE(new_encrypt_fd != NULL);

	unsigned char *ptr = (unsigned char *)calloc(input_size + TAG_SIZE,
						     sizeof(unsigned char));
	fread(ptr, sizeof(unsigned char), input_size + TAG_SIZE,
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
	unsigned char *data;
	FILE *in_file = fmemopen((void *)input, input_size, "r");
	FILE *new_file = transform_fd(in_file, NULL, &data, 0, 0);
	EXPECT_TRUE(new_file != NULL);
	EXPECT_TRUE(new_file == in_file);

	unsigned char *ptr =
	    (unsigned char *)calloc(input_size, sizeof(unsigned char));
	int read_count =
	    fread(ptr, sizeof(unsigned char), input_size, new_file);
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
	unsigned char *key = get_key(PASSPHRASE);
	unsigned char *data;
	FILE *new_encrypt_fd = transform_fd(in_file, key, &data, 1, 0);
	EXPECT_TRUE(new_encrypt_fd != NULL);

	unsigned char *ptr = (unsigned char *)calloc(input_size + TAG_SIZE,
						     sizeof(unsigned char));
	fread(ptr, sizeof(unsigned char), input_size + TAG_SIZE,
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
	unsigned char *data;
	FILE *new_compress_fd = transform_fd(in_file, NULL, &data, 0, 1);
	EXPECT_TRUE(new_compress_fd != NULL);

	unsigned char *ptr = (unsigned char *)calloc(
	    compress_bound_f(input_size), sizeof(unsigned char));
	int read_count = fread(ptr, sizeof(unsigned char),
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
	unsigned char *key = get_key(PASSPHRASE);
	unsigned char *data;
	FILE *new_encrypt_fd = transform_fd(in_file, key, &data, 1, 1);
	EXPECT_TRUE(new_encrypt_fd != NULL);

	unsigned char *ptr = (unsigned char *)calloc(
	    compress_bound_f(input_size) + TAG_SIZE, sizeof(unsigned char));
	int read_count =
	    fread(ptr, sizeof(unsigned char),
		  compress_bound_f(input_size) + TAG_SIZE, new_encrypt_fd);
	char *ptr2 = NULL;
	size_t t = 0;
	FILE *in_fd = open_memstream(&ptr2, &t);
	int ret = decode_to_fd(in_fd, key, ptr, read_count, 1, 1);
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
  unsigned char *session_key = (unsigned char *)calloc(KEY_SIZE, sizeof(unsigned char));
  //unsigned char *iv = (unsigned char *)calloc(IV_SIZE, sizeof(unsigned char));
  unsigned char *key = get_key(PASSPHRASE);

  int ret = get_decode_meta(&object_meta, session_key, key, 1, 1);
  EXPECT_EQ(ret, 0);

  printf("%s\n", object_meta.enc_session_key);

  unsigned char *back_session_key = (unsigned char *)calloc(KEY_SIZE, sizeof(unsigned char));
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

	int ret = (ret_fp) ? 0 : -1;

	EXPECT_EQ(ret, -1);
}

TEST(get_decrypt_configfpTEST, config_content_error)
{
	const char path[100] = "testpatterns/not_encrypted.conf";
	FILE* ret_fp = get_decrypt_configfp(path);

	int ret = (ret_fp) ? 0 : -1;

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
	int ret = (enc_fp) ? 0 : -1;
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
			rmdir(METAPATH);
		mkdir(METAPATH, 0700);
		ret_str = NULL;
	}

	void TearDown()
	{
		if (!access(usermeta_path, F_OK))
			unlink(usermeta_path);
		if (!access(METAPATH, F_OK))
			rmdir(METAPATH);

		free(system_config);
	}
};

TEST_F(enc_backup_usermetaTest, CreateUsermeta)
{
	EXPECT_EQ(0, enc_backup_usermeta("alohaaloha"));
	EXPECT_EQ(0, access(usermeta_path, F_OK));

	ret_str = dec_backup_usermeta(usermeta_path);
	ASSERT_NE((char *)NULL, ret_str);
	EXPECT_EQ(0, strcmp("alohaaloha", ret_str)) << ret_str;

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
	EXPECT_EQ(0, strcmp("alohaaloha", ret_str)) << ret_str;

	free(ret_str);
	unlink(usermeta_path);
}
