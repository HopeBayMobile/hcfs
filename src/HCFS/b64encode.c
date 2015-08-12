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

#include "logger.h"

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
