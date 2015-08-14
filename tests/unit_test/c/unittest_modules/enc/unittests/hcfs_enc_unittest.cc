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
