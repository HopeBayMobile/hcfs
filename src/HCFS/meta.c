/*************************************************************************
 *
 * Copyright © 2014-2016 Hope Bay Technologies, Inc. All rights reserved.
 *
 * File Name: fuseop.c
 * Abstract: The c source code file for the main FUSE operations for HCFS.
 *           All fuse functions now use fuse_reply_xxxx to pass back data
 *           and returned status, and the functions themselves do not
 *           return anything (void return type).
 *
 * Revision History
 * 2016/8/1 Jethro added init_hcfs_stat and convert_hcfsstat_to_sysstat
 *
 */

#include <assert.h>
#include <string.h>
#include <time.h>
#include "meta.h"
#include "fuseop.h"
/*
 * Erase then setup magic number and meta version on HCFS_STAT.
 *
 * @param this_stat address of HCFS_STAT struct
 *
 * @return Nothing.
 */
void init_hcfs_stat(HCFS_STAT *this_stat)
{
	memset(this_stat, 0, sizeof(HCFS_STAT));

	this_stat->metaver = CURRENT_META_VER;
	memcpy(&this_stat->magic, &META_MAGIC, sizeof(this_stat->magic));

	return;
}

/*
 * Convert HCFS_STAT to struct stat on multiple platforms.
 *
 * @param ret_stat data pointer saved in format of struct stat
 * @param tmp_stat data pointer saved in format of HCFS_STAT
 *
 * @return Nothing. ret_stat will be filled with infomation from tmp_stat
 */
void convert_hcfsstat_to_sysstat(struct stat *ret_stat, HCFS_STAT *tmp_stat)
{
	memset(ret_stat, 0, sizeof(struct stat));
#define X(MEMBER) ret_stat->st_##MEMBER = tmp_stat->MEMBER
	COMMON_STAT_MEMBER;
#undef X
#if defined(__aarch64__)
	ret_stat->st_atime      = tmp_stat->atime;
	ret_stat->st_atime_nsec = tmp_stat->atime_nsec;
	ret_stat->st_mtime      = tmp_stat->mtime;
	ret_stat->st_mtime_nsec = tmp_stat->mtime_nsec;
	ret_stat->st_ctime      = tmp_stat->ctime;
	ret_stat->st_ctime_nsec = tmp_stat->ctime_nsec;
#else
	ret_stat->st_atim.tv_sec  = tmp_stat->atime;
	ret_stat->st_atim.tv_nsec = tmp_stat->atime_nsec;
	ret_stat->st_mtim.tv_sec  = tmp_stat->mtime;
	ret_stat->st_mtim.tv_nsec = tmp_stat->mtime_nsec;
	ret_stat->st_ctim.tv_sec  = tmp_stat->ctime;
	ret_stat->st_ctim.tv_nsec = tmp_stat->ctime_nsec;
#endif
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-function-declaration"
//LCOV_EXCL_START
void static_assert_test(void) {
	_Static_assert(sizeof(HCFS_STAT)
			+ sizeof(DIR_META_TYPE)
			+ sizeof(CLOUD_RELATED_DATA)
			== sizeof(DIR_META_HEADER),
			"Makesure read all sub-struct equal to read whole header");
	_Static_assert(sizeof(HCFS_STAT)
			+ sizeof(FILE_META_TYPE)
			+ sizeof(FILE_STATS_TYPE)
			+ sizeof(CLOUD_RELATED_DATA)
			== sizeof(FILE_META_HEADER),
			"Makesure read all sub-struct equal to read whole header");
	_Static_assert(sizeof(HCFS_STAT)
			+ sizeof(SYMLINK_META_TYPE)
			+ sizeof(CLOUD_RELATED_DATA)
			== sizeof(SYMLINK_META_HEADER),
			"Makesure read all sub-struct equal to read whole header");
#define GUARDIAN_MSG "Structure size changed"
	_Static_assert(sizeof(HCFS_STAT) == 128, GUARDIAN_MSG);
	_Static_assert(sizeof(DIR_META_HEADER) == 289, GUARDIAN_MSG);
	_Static_assert(sizeof(FILE_META_HEADER) == 329, GUARDIAN_MSG);
	_Static_assert(sizeof(SYMLINK_META_HEADER) == 4357, GUARDIAN_MSG);

	/* Struct with fixed size, Do not change or remove them. */
	_Static_assert(sizeof(HCFS_STAT_v1) == 128, GUARDIAN_MSG);
	_Static_assert(sizeof(DIR_META_HEADER_v1) == 289, GUARDIAN_MSG);
	_Static_assert(sizeof(FILE_META_HEADER_v1) == 329, GUARDIAN_MSG);
	_Static_assert(sizeof(SYMLINK_META_HEADER_v1) == 4357, GUARDIAN_MSG);
}
//LCOV_EXCL_STOP
#pragma GCC diagnostic pop

/* Helper function for setting timestamp(s) to the current time, in
nanosecond precision.
   "mode" is the bit-wise OR of ATIME, MTIME, CTIME.
*/
void set_timestamp_now(HCFS_STAT *thisstat, char mode)
{
	struct timespec timenow;
	int32_t ret;

	ret = clock_gettime(CLOCK_REALTIME, &timenow);

	write_log(10, "Current time %s, ret %d\n",
		ctime(&(timenow.tv_sec)), ret);
	if (mode & ATIME) {
		thisstat->atime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
		memcpy(&(thisstat->atime), &timenow,
			sizeof(struct timespec));
#endif
	}

	if (mode & MTIME) {
		thisstat->mtime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
		memcpy(&(thisstat->mtime), &timenow,
			sizeof(struct timespec));
#endif
	}

	if (mode & CTIME) {
		thisstat->ctime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
		memcpy(&(thisstat->ctime), &timenow,
			sizeof(struct timespec));
#endif
	}
}

