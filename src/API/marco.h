/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: marco.h
* Abstract: This c header file for marcos.
*
* Revision History
* 2016/5/27 Modified after first code review.
*
**************************************************************************/

#ifndef GW20_HCFSAPI_MARCO_H_
#define GW20_HCFSAPI_MARCO_H_

#include "global.h"

/* json helper */
#define JSON_OBJ_SET_NEW(A, B, C)                                              \
	{                                                                      \
		if (json_object_set_new(A, B, C) == -1) {                      \
			goto error;                                            \
		}                                                              \
	}

/* marcos for argument operations */
#define CONCAT_ARGS(A)                                                         \
	{                                                                      \
		if (A != NULL) {                                               \
			str_len = strlen(A) + 1;                               \
			memcpy(&(buf[cmd_len]), &str_len, sizeof(ssize_t));    \
			cmd_len += sizeof(ssize_t);                            \
			memcpy(&(buf[cmd_len]), A, str_len);                   \
			cmd_len += str_len;                                    \
		}                                                              \
	}

#define CONCAT_LL_ARGS(A)                                                      \
	{                                                                      \
		memcpy(&(res_buf[ret_len]), &A, sizeof(int64_t));              \
		ret_len += sizeof(int64_t);                                    \
	}

#define CONCAT_REPLY(A, B)                                                     \
	{                                                                      \
		memcpy(resbuf + *res_size, A, B);                              \
		*res_size += B;                                                \
	}

#define READ_LL_ARGS(A)                                                        \
	{                                                                      \
		memcpy(&A, &(buf[buf_idx]), sizeof(int64_t));                  \
		buf_idx += sizeof(int64_t);                                    \
	}

/* others */
#define RETRY_SEND_EVENT(event_id)                                             \
	{                                                                      \
		retry_times = 0;                                               \
		while (retry_times < MAX_RETRY_SEND_TIMES) {                   \
			ret_code = _send_notify_event(event_id);               \
			if (ret_code == RETCODE_QUEUE_FULL) {                  \
				retry_times += 1;                              \
				continue;                                      \
			}                                                      \
			break;                                                 \
		}                                                              \
	}

#define UNUSED(x) ((void)x)

#endif  /* GW20_HCFSAPI_MARCO_H_ */
