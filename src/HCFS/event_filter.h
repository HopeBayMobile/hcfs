/*************************************************************************
*
* Copyright ©2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: event_filter.h
* Abstract: The header file for event filter functions
*
* Revision History
* 2016/7/12 Yuxun created this file.
*
**************************************************************************/

#ifndef GW20_SRC_EVENT_FILTER
#define GW20_SRC_EVENT_FILTER

#include <inttypes.h>

#define NUM_EVENTS 3
#define IS_EVENT_VALID(A) ((0 <= A) && (A < NUM_EVENTS))
/* Event IDs */
#define TESTSERVER 0
#define TOKENEXPIRED 1
#define SYNCDATACOMPLETE 2

typedef struct {
	int32_t name;
	int64_t last_send_timestamp;
	int64_t send_interval; /* Send event once in an interval (in seconds) */
} EVENT_FILTER;

extern EVENT_FILTER event_filters[];

/* Entrypoint to process all items in event filter */
int32_t check_event_filter(int32_t event_id);

/* Simple test for an item in event filter */
int32_t check_send_interval(int32_t event_id);

#endif /* GW20_SRC_EVENT_FILTER */
