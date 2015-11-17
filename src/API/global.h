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

#ifndef GW20_API_GLOBAL_H_
#define GW20_API_GLOBAL_H_

#define TRUE 1
#define FALSE 0

#define SOCK_PATH "/dev/shm/hcfs_reporter"
#define API_SOCK_PATH "/dev/shm/hcfs_api_sock"

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
#define PIN 12
#define UNPIN 13

#endif  /* GW20_API_GLOBAL_H_ */
