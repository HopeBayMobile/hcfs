/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: HCFSvol.h
* Abstract: The header file for HCFSvol
*
* Revision History
* 2015/11/17 Jethro split header file to clean style warning
*
**************************************************************************/

#ifndef GW20_SRC_HCFSVOL_H_
#define GW20_SRC_HCFSVOL_H_

#define MAX_FILENAME_LEN 255
#ifdef _ANDROID_ENV_
#define ANDROID_INTERNAL 1
#define ANDROID_EXTERNAL 2
#define ANDROID_MULTIEXTERNAL 3

#define MP_DEFAULT 1
#define MP_READ 2
#define MP_WRITE 3
#endif
/*
typedef struct {
	ino_t d_ino;
	char d_name[MAX_FILENAME_LEN+1];
	char d_type;
} DIR_ENTRY;
*/
#endif  /* GW20_SRC_HCFSVOL_H_ */
