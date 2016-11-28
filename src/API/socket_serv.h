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
#include <inttypes.h>

#define MAX_THREAD 16

typedef struct {
	uint32_t name;
	int32_t (*cmd_fn)(char *largebuf,
			  int32_t arg_len,
			  char *resbuf,
			  int32_t *res_size);
} SOCK_CMDS;

typedef struct {
	uint32_t name;
	int32_t (*cmd_fn)(char *largebuf,
			  int32_t arg_len,
			  char *resbuf,
			  int32_t *res_size,
			  pthread_t *tid);
	pthread_t async_thread;
} SOCK_ASYNC_CMDS;

typedef struct {
	char in_used;
	pthread_attr_t attr;
	pthread_t thread;
	int32_t fd;
} SOCK_THREAD;

#endif  /* GW20_HCFSAPI_SERV_H_ */
