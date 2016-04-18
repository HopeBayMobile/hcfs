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

#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>

int init_fuse_proc_communication(pthread_t *communicate_tid, int *socket_fd);
int destroy_fuse_proc_communication(pthread_t *communicate_tid, int socket_fd);
