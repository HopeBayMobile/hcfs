/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: FS_manager.c
* Abstract: The c source file for filesystem manager
*
* Revision History
* 2015/7/1 Jiahong created this file
*
**************************************************************************/

#include "FS_manager.h"

#include <stdio.h>
#include <stdlib.h>

#include "macro.h"

/* Helper function for writing head of FS manager back to file */
int sync_FS_manager_head(void)
{
/************************************************************************
*
* Function name: init_fs_manager
*        Inputs: None
*       Summary: Initialize the header for FS manager, creating the FS
*                database if it does not exist.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int init_fs_manager(void)
{
	int ret, errcode;

	fs_manager_head = malloc(sizeof(FS_MANAGER_HEAD_TYPE);
	if (fs_manager_head == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of memory in %s.\n", __func__);
		goto errcode_handle;
	}

	sem_init(&(fs_manager_head->op_lock), 0, 1);

errcode_handle:
	return errcode;
}
