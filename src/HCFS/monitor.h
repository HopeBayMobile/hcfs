/**************************************************************************
 *
 * Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
 *
 * File Name: monitor.c
 * Abstract: The c source code file for monitor backend thread and
 *           sync control (upload/download/delete).
 *
 * Revision History
 * 2015/10/30 Jethro
 *
 *************************************************************************/
#ifndef SRC_HCFS_MONITOR_H_
#define SRC_HCFS_MONITOR_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "global.h"
#include "params.h"

void *monitor_loop(void *ptr);
int32_t check_backend_status(void);
void destroy_monitor_loop_thread(void);
double diff_time(const struct timespec *start, const struct timespec *end);
void update_backend_status(register BOOL status, struct timespec *status_time);
void update_sync_state(void);
void _write_monitor_loop_status_log(double duration);

#endif  /* SRC_HCFS_MONITOR_H_ */
