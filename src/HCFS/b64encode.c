/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: b64encode.c
* Abstract: The c source code file for b64-encoding operations.
*
* Revision History
* 2015/2/10 Jiahong added header for this file, and revising coding style.
* 2015/6/2 Jiahong added error handling
*
**************************************************************************/

#include "b64encode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

unsigned char base64_codes[64] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


/************************************************************************
*
* Function name: b64encode_str
*        Inputs: unsigned char *inputstr, unsigned char *outputstr,
*                int *outlen, int inputlen
*       Summary: b64-encode input string "inputstr" (of length "inputlen")
*                to output string "outputstr" (of length "*outlen").
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int b64encode_str(unsigned char *inputstr, unsigned char *outputstr,
						int *outlen, int inputlen)
{
	unsigned char *tmpstr;
	int count, input_index, output_index;
	int origin_str_len;
	unsigned long long tmp;
	unsigned long long tmp_index;

	origin_str_len = inputlen;
	tmpstr = malloc(1+((origin_str_len+2)/3)*3);

	if (tmpstr == NULL) {
		write_log(0, "Out of memory in %s\n", __func__);
		return -ENOMEM;
	}

	memcpy(tmpstr, inputstr, origin_str_len);

	for (count = origin_str_len; count < (1+((origin_str_len+2)/3)*3);
								count++)
		tmpstr[count] = 0;

	output_index = 0;
	input_index = 0;

	for (input_index = 0; input_index < ((origin_str_len+2)/3)*3;
			input_index += 3) {
		tmp = (unsigned long long) tmpstr[input_index];
		tmp = tmp << 16;
		tmp = tmp + (((unsigned long long) tmpstr[input_index+1]) << 8);
		tmp = tmp + ((unsigned long long) tmpstr[input_index+2]);

		tmp_index = (tmp & 0xFC0000) >> 18;
		outputstr[output_index] = base64_codes[tmp_index];

		tmp_index = (tmp & 0x3F000) >> 12;
		outputstr[output_index+1] = base64_codes[tmp_index];

		if ((input_index+1) >= origin_str_len) {
			outputstr[output_index+2] = '=';
			outputstr[output_index+3] = '=';
		} else {
			tmp_index = (tmp & 0xFC0) >> 6;
			outputstr[output_index+2] = base64_codes[tmp_index];

			if ((input_index+2) >= origin_str_len) {
				outputstr[output_index+3] = '=';
			} else {
				tmp_index = (tmp & 0x3F);
				outputstr[output_index+3] =
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

static
char decode_table(char c){
	char retcode = -1;
	switch(c){
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	case 'G':
	case 'H':
	case 'I':
	case 'J':
	case 'K':
	case 'L':
	case 'M':
	case 'N':
	case 'O':
	case 'P':
	case 'Q':
	case 'R':
	case 'S':
	case 'T':
	case 'U':
	case 'V':
	case 'W':
	case 'X':
	case 'Y':
	case 'Z':
		retcode = c - 'A';
		break;
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
	case 'g':
	case 'h':
	case 'i':
	case 'j':
	case 'k':
	case 'l':
	case 'm':
	case 'n':
	case 'o':
	case 'p':
	case 'q':
	case 'r':
	case 's':
	case 't':
	case 'u':
	case 'v':
	case 'w':
	case 'x':
	case 'y':
	case 'z':
		retcode = c - 'a' + 26;
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		retcode = c - '0' + 52;
		break;
	case '+':
		retcode = 62;
		break;
	case '/':
		retcode = 63;
		break;
	case '\n':
	case '\r':
	case '\t':
	case ' ':
	case '=':
	case '\0':
		retcode = 64; // ignore
		break;
	default:
		retcode = -1;
		break;
	}
	return retcode;
}


/************************************************************************
*
* Function name: b64decode_str
*        Inputs: char *inputstr, unsigned char *outputstr,
*                int *outlen, int inputlen
*       Summary: b64-decode input string "inputstr" (of length "inputlen")
*                to output string "outputstr" (of length "*outlen").
*  Return value: 0 if successful.
*                -1 if illegal character occurs
*                -2 if impossible format occurs
*
*************************************************************************/
int b64decode_str(char *inputstr, unsigned char *outputstr,
		  int *outlen, int inputlen){
	int i = 0;
	int out_index = 0;
	int group_count = 0;
	char buf[4] = {0};
	while(i < inputlen){
		char decode = decode_table(inputstr[i++]);
		if(decode == -1){
			// not allowed characters occurs
			return -1;
		}
		else if(decode == 64){
			continue;
		}
		else{
			buf[group_count++] = decode;
			if(group_count == 4){
				group_count = 0;
				outputstr[out_index++] = (buf[0] << 2) +
					((buf[1] & 0x30) >> 4);
				outputstr[out_index++] = (buf[1] << 4) +
					(buf[2] >> 2);
				outputstr[out_index++] = (buf[2] << 6) +
					buf[3];
			}
		}
	}
	if(group_count == 3){
		outputstr[out_index++] = (buf[0] << 2) + ((buf[1] & 0x30) >> 4);
		outputstr[out_index++] = (buf[1] << 4) + (buf[2] >> 2);
		outputstr[out_index++] = buf[2] << 6;
	}
	else if(group_count == 2){
		outputstr[out_index++] = (buf[0] << 2) + ((buf[1] & 0x30) >> 4);
		outputstr[out_index++] = (buf[1] << 4);
	}
	else{
		// impossible situation
		return -2;
	}
	*outlen = out_index - 1;
	return 0;
}
