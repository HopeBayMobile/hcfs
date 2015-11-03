/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: global.h
* Abstract: The header file for global settings for HCFS
*
* Revision History
* 2015/2/11 Jiahong added header for this file and revised coding style.
* 2015/7/21 Jiahong moving API codes to global.h
*
**************************************************************************/

#ifndef GW20_SRC_GLOBAL_H_
#define GW20_SRC_GLOBAL_H_

#define TRUE 1
#define FALSE 0

/* Defines the version of the current meta defs */
#define CURRENT_META_VER 3
#define BACKWARD_COMPATIBILITY 3
/* TODO: force backward compability check when reading meta file */

#define X64 1
#define ARM_32BIT 2
#define ANDROID_32BIT 3

#define ARCH_CODE ANDROID_32BIT

/* List of API codes */
#define TERMINATE 0
#define VOLSTAT 1
#define TESTAPI 2
#define ECHOTEST 3
#define CREATEFS 4
#define MOUNTFS 5
#define DELETEFS 6
#define CHECKFS 7
#define LISTFS 8
#define UNMOUNTFS 9
#define CHECKMOUNT 10
#define UNMOUNTALL 11

/* Print format for ino_t is different in raspberry pi */
#ifdef ARM_32bit_
#define FMT_INO_T "lld"
#else
#define FMT_INO_T "ld"
#endif

#endif  /* GW20_SRC_GLOBAL_H_ */
