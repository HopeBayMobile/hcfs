#include <pthread.h>

#define MAX_THREAD 16

typedef struct {
	char in_used;
	pthread_t thread;
	int fd;
} SOCK_THREAD;
