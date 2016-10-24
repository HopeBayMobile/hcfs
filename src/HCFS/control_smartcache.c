/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: control_smartcache.c
* Abstract: The c source code file for controlling mounting of smart cache.
*
* Revision History
* 2016/10/20 Kewei created this file.
*
**************************************************************************/
#include "control_smartcache.h"

#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "macro.h"
#include "logger.h"

int32_t unmount_smart_cache()
{
	int32_t ret;

	ret = umount(SMART_CACHE_PATH);
	if (ret < 0)
		ret = -errno;

	return ret;
}

int32_t mount_smart_cache()
{
	int32_t ret;
	int32_t errcode;

	if (access(SMART_CACHE_PATH, F_OK) < 0) {
		errcode = -errno;
		if (errcode == -ENOENT) {
			write_log(6, "Create folder %s", SMART_CACHE_PATH);
			MKDIR(SMART_CACHE_PATH, 0);
		} else {
			goto errcode_handle;
		}
	}

	ret = mount("/dev/block/loop7", SMART_CACHE_PATH, "ext4", 0, NULL);
	if (ret < 0)
		ret = -errno;

	return ret;

errcode_handle:
	return errcode;
}
