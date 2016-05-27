/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: pin_ops.h
* Abstract: This c header file for pin related operations.
*
* Revision History
* 2016/5/27 Modified after first code review.
*
**************************************************************************/

#ifndef GW20_HCFSAPI_PIN_H_
#define GW20_HCFSAPI_PIN_H_

#include <inttypes.h>

int32_t pin_by_path(char *buf, uint32_t arg_len);

int32_t unpin_by_path(char *buf, uint32_t arg_len);

int32_t check_pin_status(char *buf, uint32_t arg_len);

int32_t check_dir_status(char *buf, uint32_t arg_len,
			int64_t *num_local, int64_t *num_cloud,
			int64_t *num_hybrid);

int32_t check_file_loc(char *buf, uint32_t arg_len);

#endif  /* GW20_HCFSAPI_PIN_H_ */
