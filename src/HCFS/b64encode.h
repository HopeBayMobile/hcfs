/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: b64encode.h
* Abstract: The c header file for b64-encoding operations.
*
* Revision History
* 2015/2/10 Jiahong created this file and moved related function from
*           hcfscurl.h.
* 2015/2/10 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/
#ifndef GW20_HCFS_B64ENCODE_H_
#define GW20_HCFS_B64ENCODE_H_

int32_t b64encode_str(uint8_t *inputstr, uint8_t *outputstr,
						int32_t *outlen, int32_t inputlen);

int32_t b64decode_str(char *inputstr, uint8_t *outputstr,
						int32_t *outlen, int32_t inputlen);
#endif  /* GW20_HCFS_B64ENCODE_H_ */
