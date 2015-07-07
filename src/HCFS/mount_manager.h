/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: FS_manager.h
* Abstract: The header file for mount manager
*
* Revision History
* 2015/7/7 Jiahong created this file
*
**************************************************************************/

/* Routines should also lock FS manager if needed to prevent inconsistency
in the two modules */

int FS_is_mounted(char *fsname);
