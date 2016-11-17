/*************************************************************************
*
* Copyright ©2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: event_filter.c
* Abstract: The c source file for event filter functions
*
* Revision History
* 2016/7/12 Yuxun created this file.
*
**************************************************************************/

#include "event_filter.h"

#include <time.h>

/* Register event filter here */
EVENT_FILTER event_filters[] = {
	/* name, last_send_timestamp, send_interval */
	{TESTSERVER,	   0,	  0},
	{TOKENEXPIRED,	   0,	120},
	{SYNCDATACOMPLETE, 0,     0},
	{RESTORATION_STAGE1_CALLBACK,	0,	0},
	{RESTORATION_STAGE2_CALLBACK,	0,	0},
	{TRIGGER_BOOST_SUCCESS,	0,	0},
	{TRIGGER_BOOST_FAILED,	0,	0},
};


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

