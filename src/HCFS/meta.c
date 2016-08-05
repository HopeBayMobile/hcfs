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
#include "meta.h"
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
void static_assert_test(void) {
	static_assert(sizeof(HCFS_STAT)
			+ sizeof(DIR_META_TYPE)
			+ sizeof(CLOUD_RELATED_DATA)
			+ sizeof(uint8_t) * 64
			== sizeof(DIR_META_HEADER),
			"Makesure read all sub-struct equal to read whole header");
	static_assert(sizeof(HCFS_STAT)
			+ sizeof(FILE_META_TYPE)
			+ sizeof(FILE_STATS_TYPE)
			+ sizeof(CLOUD_RELATED_DATA)
			+ sizeof(uint8_t) * 64
			== sizeof(FILE_META_HEADER),
			"Makesure read all sub-struct equal to read whole header");
	static_assert(sizeof(HCFS_STAT)
			+ sizeof(SYMLINK_META_TYPE)
			+ sizeof(CLOUD_RELATED_DATA)
			== sizeof(SYMLINK_META_HEADER),
			"Makesure read all sub-struct equal to read whole header");
#define GUARDIAN_MSG "Structure size changed"
	static_assert(sizeof(HCFS_STAT) == 128, GUARDIAN_MSG);
	static_assert(sizeof(DIR_META_HEADER) == 296, GUARDIAN_MSG);
	static_assert(sizeof(FILE_META_HEADER) == 336, GUARDIAN_MSG);
	static_assert(sizeof(SYMLINK_META_HEADER) == 4304, GUARDIAN_MSG);

	/* Struct with fixed size, Do not change or remove them. */
	static_assert(sizeof(DIR_META_HEADER_v1) == 296, GUARDIAN_MSG);
	static_assert(sizeof(FILE_META_HEADER_v1) == 336, GUARDIAN_MSG);
	static_assert(sizeof(SYMLINK_META_HEADER_v1) == 4304, GUARDIAN_MSG);
	static_assert(sizeof(HCFS_STAT_v1) == 128, GUARDIAN_MSG);
}
#pragma GCC diagnostic pop
