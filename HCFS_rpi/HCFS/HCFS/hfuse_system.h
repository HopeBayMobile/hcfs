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

int init_hfuse(void);
int init_hcfs_system_data(void);
int sync_hcfs_system_data(char need_lock);

#endif  /* GW20_SRC_HFUSE_SYSTEM_H_ */
