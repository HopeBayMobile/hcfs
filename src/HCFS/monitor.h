/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfs_clouddelete.h
* Abstract: The c header file for deleting meta or data from
*           backend.
*
* Revision History
* 2015/2/13 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/
#ifndef GW20_HCFS_MONITOR_H_
#define GW20_HCFS_MONITOR_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _ANDROID_ENV_
void *monitor_loop(void *ptr);
#else
void monitor_loop(void);
#endif
struct timespec diff_time(struct timespec start, struct timespec end);

#endif  /* GW20_HCFS_MONITOR_H_ */
