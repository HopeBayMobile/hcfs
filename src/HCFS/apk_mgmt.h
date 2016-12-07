/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: apk_mgmt.h
* Abstract: The header file to control Android app pin/unpin status.
*
* Revision History
* 2016/12/07 Jethro created this file.
*
**************************************************************************/

#ifndef SRC_HCFS_APK_MGMT_H_
#define SRC_HCFS_APK_MGMT_H_

#include <stdbool.h>
#include <inttypes.h>

int32_t toggle_use_minimal_apk(bool new_val);
int32_t initialize_minimal_apk(void);
int32_t terminate_minimal_apk(void);

#endif  /* SRC_HCFS_APK_MGMT_H_ */
