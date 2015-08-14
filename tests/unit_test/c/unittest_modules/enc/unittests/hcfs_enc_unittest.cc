#include <gtest/gtest.h>
#include <string.h>
extern "C" {
#include "b64encode.h"
#include "enc.h"
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

TEST(enc, encrypt_with_fix_iv)
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

TEST(enc, encrypt_then_decrypt)
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


}
