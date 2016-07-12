/*************************************************************************
*
* Copyright Â2016 Hope Bay Technologies, Inc. All rights reserved.
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

EVENT_FILTER event_filters[] = {
	/* name, last_send_timestamp, send_interval */
	{TESTSERVER,	0,	0},
	{TOKENEXPIRED,	0,	120}
};

int32_t check_event_filter(int32_t event_id)
{
	int32_t ret_code;

	ret_code = check_send_interval(event_id);
	if (ret_code < 0)
		return ret_code;
}

int32_t check_send_interval(int32_t event_id)
{
	return -1;
}

