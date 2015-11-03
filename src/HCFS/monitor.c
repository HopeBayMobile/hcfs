/**************************************************************************
 *
 * Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
 *
 * File Name: monitor.c
 * Abstract: The c source code file for monitor backend connection thread and
 *           sync control (upload/download/delete).
 *
 * Revision History
 * 2015/10/30 Jethro
 *
 *************************************************************************/

#include "monitor.h"

#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/prctl.h>

#include "fuseop.h"
#include "global.h"
#include "logger.h"

/**************************************************************************
 *
 * Function name: monitor_loop
 *        Inputs: void *arg
 *       Summary: Main function for checking whether there is a need to
 *                delete objects from backend.
 *  Return value: None
 *
 *************************************************************************/
void monitor_loop()
{
#ifdef _ANDROID_ENV_
	prctl(PR_SET_NAME, "monitor_loop");
#endif /* _ANDROID_ENV_ */

	write_log(2, "Start monitor loop\n");

	while (hcfs_system->system_going_down == FALSE) {
		sleep(1);
	}
}
