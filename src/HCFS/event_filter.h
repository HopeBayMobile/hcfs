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
       TRIGGER_BOOST_SUCCESS,
       TRIGGER_BOOST_FAILED,
       NUM_EVENTS } EVENT_TYPE;
#define IS_EVENT_VALID(A) ((0 <= A) && (A < NUM_EVENTS))

typedef enum {
	MEDIA_TYPE_IMAGE = 0,
	MEDIA_TYPE_VIDEO,
	MEDIA_TYPE_NON_MEDIA
} MEDIA_TYPE;

/* To declare all events */
#define REGISTER_EVENTS                                                        \
	EVENT_FILTER event_filters[] = {                                       \
		{ TESTSERVER, 0, 0 },                                          \
		{ TOKEN_EXPIRED, 0, 120 },                                     \
		{ SYNCDATACOMPLETE, 0, 0 },                                    \
		{ RESTORATION_STAGE1_CALLBACK, 0, 0 },                         \
		{ RESTORATION_STAGE2_CALLBACK, 0, 0 },                         \
		{ EXCEED_PIN_MAX, 0, 0 },                                      \
		{ SPACE_NOT_ENOUGH, 0, 0 },                                    \
		{ CREATE_THUMBNAIL, 0, 0 },                                    \
		{ TRIGGER_BOOST_SUCCESS, 0, 0 },                               \
		{ TRIGGER_BOOST_FAILED, 0, 0 },                                \
	};

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
