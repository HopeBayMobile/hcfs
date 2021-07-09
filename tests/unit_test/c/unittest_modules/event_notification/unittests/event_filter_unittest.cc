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

/* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved. */
#include <gtest/gtest.h>
#include <errno.h>
extern "C" {
#include "global.h"
#include "fuseop.h"
#include "time.h"
#include "event_filter.h"
}

/* Unittests for check_event_filter */
TEST(check_event_filterTEST, SendAllowed)
{
	event_filters[0].last_send_timestamp = 0;
	event_filters[0].send_interval = 0;
	EXPECT_EQ(check_event_filter(0), 0);
}
TEST(check_event_filterTEST, CheckEventFilterBlocked)
{
	event_filters[0].last_send_timestamp =
		(int64_t)time(NULL) - 1;
	event_filters[0].send_interval = 999;
	EXPECT_EQ(check_event_filter(0), -1);
}
/* End unittests for check_event_filter */

/* Unittests for check_event_filter */
TEST(check_send_intervalTEST, SendAllowed)
{
	event_filters[0].last_send_timestamp = 0;
	event_filters[0].send_interval = 0;
	EXPECT_EQ(check_send_interval(0), 0);
}
TEST(check_send_intervalTEST, SendBlocked)
{
	event_filters[0].last_send_timestamp =
		(int64_t)time(NULL) - 1;
	event_filters[0].send_interval = 999;
	EXPECT_EQ(check_send_interval(0), -1);
}
/* End unittests for check_send_interval */
