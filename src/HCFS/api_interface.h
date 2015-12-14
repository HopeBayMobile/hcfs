/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: api_interface.h
* Abstract: The header file for Defining API for controlling / monitoring
*
* Revision History
* 2015/6/10 Jiahong created this file.
* 2015/7/21 Jiahong moving API codes to global.h
*
**************************************************************************/

#ifndef GW20_API_INTERFACE_H_
#define GW20_API_INTERFACE_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <semaphore.h>
#include <time.h>

#ifdef _ANDROID_ENV_
#include <pthread.h>
#endif

#define MAX_API_THREADS 10
#define INIT_API_THREADS 4
#define PROCESS_WINDOW 30
#define INCREASE_RATIO 0.8
#define SOCK_PATH "/dev/shm/hcfs_reporter"

/* Message format for an API request:
	(From the first byte)
	API code, size (unsigned int)
	Total length of arguments, size (unsigned int)
	Arguments (To be stored in a char array and pass to the handler
		for each API)

   Message format for an API response:
	(From the first byte)
	Total length of response, size (unsigned int)
	Response (as a char string)
*/

typedef struct {
	struct sockaddr_un addr;
	int fd;
} SOCKET;

typedef struct {
	SOCKET sock;
	/* API thread (using local socket) */
	pthread_t local_thread[MAX_API_THREADS];
	pthread_t monitor_thread;
	int num_threads;
	int job_count[PROCESS_WINDOW];
	float job_totaltime[PROCESS_WINDOW];
	time_t last_update;
	sem_t job_lock;
} API_SERVER_TYPE;

API_SERVER_TYPE *api_server;

int init_api_interface(void);
int destroy_api_interface(void);
void api_module(void *index);
void api_server_monitor(void);

#endif  /* GW20_API_INTERFACE_H_ */
