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

#include <stdint.h>

#define BOOL uint8_t
#define TRUE 1
#define FALSE 0
#define ON 1
#define OFF 0

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
#define CREATEVOL 4
#define MOUNTVOL 5
#define DELETEVOL 6
#define CHECKVOL 7
#define LISTVOL 8
#define UNMOUNTVOL 9
#define CHECKMOUNT 10
#define UNMOUNTALL 11
#define PIN 12
#define UNPIN 13
#define CHECKDIRSTAT 14
#define GETVOLSIZE 15
#define GETCLOUDSIZE 16
#define GETPINSIZE 17
#define GETCACHESIZE 18
#define CHECKLOC 19
#define CHECKPIN 20
#define GETMAXPINSIZE 21
#define GETMAXCACHESIZE 22
#define CLOUDSTAT 23
#define GETDIRTYCACHESIZE 24
#define GETXFERSTAT 25
#define RESETXFERSTAT 26
#define SETSYNCSWITCH 27
#define GETSYNCSWITCH 28
#define GETSYNCSTAT 29
#define RELOADCONFIG 30

#define DEFAULT_PIN TRUE

#ifdef _ANDROID_ENV_
#define SDCARD_RW 1015
#define EVERYBODY 1000
#endif

#endif  /* GW20_SRC_GLOBAL_H_ */
