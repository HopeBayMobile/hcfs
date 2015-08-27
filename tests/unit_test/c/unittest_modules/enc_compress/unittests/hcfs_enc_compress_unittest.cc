#include <gtest/gtest.h>
#include <string.h>
#include <stdio.h>
extern "C" {
#include "params.h"
#include "b64encode.h"
#include "enc.h"
#include "compress.h"
}


class enc : public testing::Test{
protected:
	virtual void SetUp(){
		system_config.max_block_size = 1073741824;
	}
};

TEST(compress, compress)
{
	const char input[1000] = {0};
	int input_size = 1000;
	int output_max_size = compress_bound_f(input_size);
	char* compressed = (char*)calloc(output_max_size,
						 sizeof(char));
	int compress_size = compress_f(input, compressed, input_size);
	EXPECT_TRUE(compress_size < input_size);
	char* back = (char*)calloc(1073741824,
				 sizeof(char));
	int back_size = decompress_f(compressed, back, compress_size,
				     1073741824);
	EXPECT_TRUE(back_size == input_size);
	EXPECT_EQ(memcmp(input, back, input_size), 0);
	free(compressed);
	free(back);
}


TEST(base64, encode_then_decode)
{
	const char* input = "4ytg0jkk0234]yhj]-43jddfv1111";
	int length_encode = expect_b64_encode_length(strlen(input));
	char* code = (char*)calloc(length_encode, sizeof(char));
	int out_len = 0;
	b64encode_str((unsigned char*)input, (unsigned char*)code, &out_len, strlen(input));
	unsigned char* decode = (unsigned char*)calloc(out_len+1, sizeof(unsigned char));
	b64decode_str(code, decode, &out_len, out_len);
	EXPECT_EQ(memcmp(input, decode, strlen(input)), 0);
	free(code);
	free(decode);

}

TEST_F(enc, encrypt_with_fix_iv)
{
	const char* input = "4ytg0jkk0234]yhj]-43jddfv1111";
	unsigned char* key = get_key();
	unsigned char iv[IV_SIZE] = {0};
	unsigned char* output1 = (unsigned char*)calloc(strlen(input)+TAG_SIZE,
							sizeof(unsigned char));
	unsigned char* output2 = (unsigned char*)calloc(strlen(input)+TAG_SIZE,
							sizeof(unsigned char));
	int ret1 = aes_gcm_encrypt_core(output1, (unsigned char*)input, strlen(input), key, iv);
	int ret2 = aes_gcm_encrypt_fix_iv(output2, (unsigned char*)input, strlen(input), key);
	EXPECT_EQ(ret1, 0);
	EXPECT_EQ(ret2, 0);
	EXPECT_EQ(memcmp(output1, output2, strlen(input)+TAG_SIZE), 0);
	free(output1);
	free(output2);
	free(key);
}

TEST_F(enc, encrypt_then_decrypt)
{
	const char* input = "4ytg0jkk0234]yhj]-43jddfv1111";
	unsigned char* key = get_key();
	unsigned char* output = (unsigned char*)calloc(strlen(input)+TAG_SIZE,
							sizeof(unsigned char));
	unsigned char* decode = (unsigned char*)calloc(strlen(input),
							sizeof(unsigned char));

	int ret = aes_gcm_encrypt_fix_iv(output, (unsigned char*)input, strlen(input), key);
	EXPECT_EQ(ret, 0);
	ret = aes_gcm_decrypt_fix_iv(decode, output, strlen(input)+TAG_SIZE,
					 key);
	EXPECT_EQ(ret, 0);
	EXPECT_EQ(memcmp(input, decode, strlen(input)), 0);
	free(key);
	free(output);
	free(decode);
}

TEST_F(enc, transform_encrypt_fd)
{

	const char* input = "4ytg0jkk0234]yhj]-43jddfv1111";
	FILE* in_file = fmemopen((void*)input, strlen(input), "r");
	unsigned char* key = get_key();
	unsigned char* data;
	FILE* new_encrypt_fd = transform_encrypt_fd(in_file, key, &data);
	EXPECT_TRUE( new_encrypt_fd != NULL);

	unsigned char *ptr = (unsigned char*)calloc(strlen(input)+TAG_SIZE,
					   sizeof(unsigned char));
	fread(ptr, sizeof(unsigned char),
	      strlen(input)+TAG_SIZE, new_encrypt_fd);
	char *ptr2 = NULL;
	size_t t = 0;
	FILE *in_fd = open_memstream(&ptr2, &t);
	decrypt_to_fd(in_fd, key, ptr, strlen(input)+TAG_SIZE);

	fclose(in_fd);
	EXPECT_EQ(memcmp(ptr2, input, strlen(input)), 0);

	fclose(new_encrypt_fd);
	fclose(in_file);
	free(key);
	free(data);
	free(ptr);
	free(ptr2);

}
