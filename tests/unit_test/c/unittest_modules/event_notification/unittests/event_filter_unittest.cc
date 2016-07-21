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
