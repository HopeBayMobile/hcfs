/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: apk_mgmt.c
* Abstract: The c file to control Android app pin/unpin status.
*
* Revision History
* 2016/12/07 Jethro created this file.
*
**************************************************************************/

#include "apk_mgmt.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>

#include "fuseop.h"
#include "global.h"
#include "hfuse_system.h"
#include "macro.h"
#include "params.h"

int32_t toggle_use_minimal_apk(bool new_val)
{
	int32_t ret = 0;
	bool old_val = hcfs_system->use_minimal_apk;

	if (old_val == false && new_val == true)
		ret = initialize_minimal_apk();
	else if (old_val == true && new_val == false)
		ret = terminate_minimal_apk();

	return ret;
}

int32_t initialize_minimal_apk(void) {
	/* Enable use_minimal_apk after everything ready */
	hcfs_system->use_minimal_apk = true;
	return 0;
}
int32_t terminate_minimal_apk(void) {
	/* Disable use_minimal_apk first, then destroy everything */
	hcfs_system->use_minimal_apk = false;
	return 0;
}
