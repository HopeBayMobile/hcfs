#ifndef GW20_API_SERV_H_
#define GW20_API_SERV_H_

#include <pthread.h>

#define MAX_THREAD 16


typedef struct {
	char in_used;
	pthread_attr_t attr;
	pthread_t thread;
	int fd;
} SOCK_THREAD;

#endif  /* GW20_API_SERV_H_ */
