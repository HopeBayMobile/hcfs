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
