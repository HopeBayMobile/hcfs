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

#include "event_filter.h"

#include <time.h>

/* Register event filter here */
REGISTER_EVENTS;

/************************************************************************
 *
 *  Function name: check_event_filter
 *         Inputs: int32_t event_id
 *         Output: Integer
 *        Summary: Entry point to check all items in event filter.
 *   Return value: 0 if event is allowed to send.
 *                -1 if not allowed.
 *
 ***********************************************************************/
int32_t check_event_filter(int32_t event_id)
{
	int32_t ret_code;

	ret_code = check_send_interval(event_id);
	if (ret_code < 0)
		return ret_code;

	return 0;
}

/************************************************************************
 *
 *  Function name: check_send_interval
 *         Inputs: int32_t event_id
 *         Output: Integer
 *        Summary: To check send interval. An event can only be send
 *                 once in send interval.
 *   Return value: 0 if send event is allowed.
 *                -1 if this event had been send in this interval.
 *
 ***********************************************************************/
int32_t check_send_interval(int32_t event_id)
{
	uint32_t idx;
	int64_t now_ts, timedelta;

	for (idx = 0; idx < sizeof(event_filters) / sizeof(event_filters[0]);
	     idx++) {
		if (event_id == event_filters[idx].name) {
			now_ts = (int64_t)time(NULL);
			timedelta = (int64_t)difftime(
			    now_ts, event_filters[idx].last_send_timestamp);
			if (timedelta < event_filters[idx].send_interval)
				return -1;
			else
				return 0;
		}
	}
	return 0;
}

