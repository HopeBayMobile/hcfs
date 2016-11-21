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

/* Event IDs */
enum { TESTSERVER = 0,
       TOKEN_EXPIRED,
       SYNCDATACOMPLETE,
       RESTORATION_STAGE1_CALLBACK,
       RESTORATION_STAGE2_CALLBACK,
       EXCEED_PIN_MAX,
       SPACE_NOT_ENOUGH,
       CREATE_THUMBNAIL,
       NUM_EVENTS } EVENT_TYPE;
#define IS_EVENT_VALID(A) ((0 <= A) && (A < NUM_EVENTS))

typedef enum {
	MEDIA_TYPE_IMAGE = 0,
	MEDIA_TYPE_VIDEO,
	MEDIA_TYPE_NON_MEDIA
} MEDIA_TYPE;

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
