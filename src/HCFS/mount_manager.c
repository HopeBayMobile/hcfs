/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: FS_manager.c
* Abstract: The C source file for mount manager
*
* Revision History
* 2015/7/7 Jiahong created this file
*
**************************************************************************/

#include "mount_manager.h"

#include "global.h"

/* Routines should also lock FS manager if needed to prevent inconsistency
in the two modules */

int FS_is_mounted(char *fsname)
{

	return FALSE;
}
