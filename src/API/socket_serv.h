/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: socket_serv.h
* Abstract: This c header file for HCFSAPI socket server.
*
* Revision History
* 2016/5/27 Modified after first code review.
*
**************************************************************************/

#ifndef GW20_HCFSAPI_SERV_H_
#define GW20_HCFSAPI_SERV_H_

#include <pthread.h>

#define MAX_THREAD 16


typedef struct {
	char in_used;
	pthread_attr_t attr;
	pthread_t thread;
	int32_t fd;
} SOCK_THREAD;

#endif  /* GW20_HCFSAPI_SERV_H_ */
