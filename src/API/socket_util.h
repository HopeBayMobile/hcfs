/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: socket_util.h
* Abstract: This c header file for socket helpers.
*
* Revision History
* 2016/5/27 Modified after first code review.
*
**************************************************************************/

#ifndef GW20_HCFSAPI_SOCKUTIL_H_
#define GW20_HCFSAPI_SOCKUTIL_H_

int32_t reads(int32_t fd, char *buf, int32_t count);

int32_t sends(int32_t fd, const char *buf, int32_t count);

#endif /* GW20_HCFSAPI_SOCKUTIL_H_ */
