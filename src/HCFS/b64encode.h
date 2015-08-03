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

int b64encode_str(unsigned char *inputstr, unsigned char *outputstr,
						int *outlen, int inputlen);

int b64decode_str(char *inputstr, unsigned char *outputstr,
						int *outlen, int inputlen);
#endif  /* GW20_HCFS_B64ENCODE_H_ */
