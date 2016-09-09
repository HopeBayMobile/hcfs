/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hfuse_system.h
* Abstract: The c header file for HCFS system main function.
*
* Revision History
* 2015/2/11 Jiahong created this file by moving some definition from
*          fuseop.h.
*
**************************************************************************/

#ifndef GW20_SRC_HFUSE_SYSTEM_H_
#define GW20_SRC_HFUSE_SYSTEM_H_

#include "params.h"
#include "pthread.h"
#include "inttypes.h"

int32_t init_hfuse(void);
int32_t init_hcfs_system_data(void);
int32_t sync_hcfs_system_data(char need_lock);
void init_backend_related_module(void);
int32_t init_event_notify_module(void);

#define CHILD_NUM 2
#define BATTERY_LOW_LEVEL 3

pthread_t delete_loop_thread;
pthread_t monitor_loop_thread;
pthread_t event_loop_thread;
pthread_t fuse_nofify_thread;
#ifdef _ANDROID_ENV_
pthread_t upload_loop_thread;
pthread_t cache_loop_thread;
#else
pid_t child_pids[CHILD_NUM];
pid_t this_pid;
int32_t proc_idx;
#endif /* _ANDROID_ENV_ */

#endif  /* GW20_SRC_HFUSE_SYSTEM_H_ */
