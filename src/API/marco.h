/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
				sleep(1);                                      \
				retry_times += 1;                              \
				continue;                                      \
			}                                                      \
			break;                                                 \
		}                                                              \
	}

#define UNUSED(x) ((void)x)

#endif  /* GW20_HCFSAPI_MARCO_H_ */
