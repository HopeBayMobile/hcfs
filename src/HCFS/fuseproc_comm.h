/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: fuseproc_comm.h
* Abstract: The header file for fuseproc_comm.c definition
*
* Revision History
* 2016/4/18 Kewei created this file.
*
**************************************************************************/

#ifndef GW20_FUSEPROC_COMM_H_
#define GW20_FUSEPROC_COMM_H_

#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>

/* Define socket path for process communicating */
#define MAX_FUSE_COMMUNICATION_THREAD 4

int32_t init_fuse_proc_communication(pthread_t *communicate_tid, int32_t *socket_fd);
int32_t destroy_fuse_proc_communication(pthread_t *communicate_tid, int32_t socket_fd);
#endif
