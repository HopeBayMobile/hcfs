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

#include "b64encode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "logger.h"

const char base64_codes[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/************************************************************************
*
* Function name: b64encode_str
*        Inputs: uint8_t *inputstr, char *outputstr,
*                int32_t *outlen, int32_t inputlen
*       Summary: b64-encode input string "inputstr" (of length "inputlen")
*                to output string "outputstr" (of length "*outlen").
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t b64encode_str(uint8_t *inputstr, char *outputstr,
		  int32_t *outlen, int32_t inputlen)
{
	uint8_t *tmpstr;
	int32_t count, input_index, output_index;
	int32_t origin_str_len;
	uint64_t tmp;
	uint64_t tmp_index;

	origin_str_len = inputlen;
	tmpstr = malloc(1 + ((origin_str_len + 2) / 3) * 3);

	if (tmpstr == NULL) {
		write_log(0, "Out of memory in %s\n", __func__);
		return -ENOMEM;
	}

	memcpy(tmpstr, inputstr, origin_str_len);

	for (count = origin_str_len;
	     count < (1 + ((origin_str_len + 2) / 3) * 3); count++)
		tmpstr[count] = 0;

	output_index = 0;

	for (input_index = 0; input_index < ((origin_str_len + 2) / 3) * 3;
	     input_index += 3) {
		tmp = (uint64_t)tmpstr[input_index];
		tmp = tmp << 16;
		tmp =
		    tmp + (((uint64_t)tmpstr[input_index + 1]) << 8);
		tmp = tmp + ((uint64_t)tmpstr[input_index + 2]);

		tmp_index = (tmp & 0xFC0000) >> 18;
		outputstr[output_index] = base64_codes[tmp_index];

		tmp_index = (tmp & 0x3F000) >> 12;
		outputstr[output_index + 1] = base64_codes[tmp_index];

		if ((input_index + 1) >= origin_str_len) {
			outputstr[output_index + 2] = '=';
			outputstr[output_index + 3] = '=';
		} else {
			tmp_index = (tmp & 0xFC0) >> 6;
			outputstr[output_index + 2] = base64_codes[tmp_index];

			if ((input_index + 2) >= origin_str_len) {
				outputstr[output_index + 3] = '=';
			} else {
				tmp_index = (tmp & 0x3F);
				outputstr[output_index + 3] =
				    base64_codes[tmp_index];
			}
		}
		output_index += 4;
	}

	outputstr[output_index] = 0;

	*outlen = output_index;

	free(tmpstr);

	return 0;
}

static char decode_table(char c)
{
	if (c >= 'A' && c <= 'Z') {
		return c - 'A';
	} else if (c >= 'a' && c <= 'z') {
		return c - 'a' + 26;
	} else if (c >= '0' && c <= '9') {
		return c - '0' + 52;
	} else {
		switch (c) {
		case '+':
			return 62;
		case '/':
			return 63;
		case '\n':
		case '\r':
		case '\t':
		case ' ':
		case '=':
		case '\0':
			return 64; /* ignore */
		default:
			return -1;
		}
	}
}

/************************************************************************
*
* Function name: b64decode_str
*        Inputs: char *inputstr, uint8_t *outputstr,
*                int32_t *outlen, int32_t inputlen
*       Summary: b64-decode input string "inputstr" (of length "inputlen")
*                to output string "outputstr" (of length "*outlen").
*  Return value: 0 if successful.
*                -1 if illegal character occurs
*                -2 if impossible format occurs
*
*************************************************************************/
int32_t b64decode_str(char *inputstr, uint8_t *outputstr, int32_t *outlen,
		  int32_t inputlen)
{
	int32_t i = 0;
	int32_t out_index = 0;
	int32_t group_count = 0;
	char buf[4] = {0};

	while (i < inputlen) {
		signed char decode = decode_table(inputstr[i++]);

		if (decode == -1) {
			/* not allowed characters occurs */
			return -1;
		} else if (decode == 64) {
			continue;
		} else {
			buf[group_count++] = decode;
			if (group_count == 4) {
				group_count = 0;
				outputstr[out_index++] =
				    (buf[0] << 2) + ((buf[1] & 0x30) >> 4);
				outputstr[out_index++] =
				    (buf[1] << 4) + (buf[2] >> 2);
				outputstr[out_index++] = (buf[2] << 6) + buf[3];
			}
		}
	}
	if (group_count == 3) {
		outputstr[out_index++] = (buf[0] << 2) + ((buf[1] & 0x30) >> 4);
		outputstr[out_index++] = (buf[1] << 4) + (buf[2] >> 2);
		outputstr[out_index++] = buf[2] << 6;
	} else if (group_count == 2) {
		outputstr[out_index++] = (buf[0] << 2) + ((buf[1] & 0x30) >> 4);
		outputstr[out_index++] = (buf[1] << 4);
	} else if (group_count != 0) {
		/* impossible situation */
		return -2;
	}
	*outlen = out_index - 1;
	return 0;
}
