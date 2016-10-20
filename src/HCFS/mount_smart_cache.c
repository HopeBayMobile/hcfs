/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: mount_smart_cache.c
* Abstract: The c source code file for controlling mounting of smart cache.
*
* Revision History
* 2016/10/20 Kewei created this file.
*
**************************************************************************/
#include "mount_smart_cache.h"

#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"

int32_t unmount_smart_cache()
{
	char exec_command[400] = {0};
	int32_t ret;

	strcpy(exec_command, "unmount ");
	strcat(exec_command, SMART_CACHE_PATH);
	ret = system(exec_command);
	if (ret < 0) {
		write_log(0, "Fail to exec command in %s. Code %d",
				__func__, errno);
		goto out;
	}

	if (WIFEXITED(ret)) {
		ret = WEXITSTATUS(ret);
	} else if (WIFSIGNALED(ret)) {
		ret = WTERMSIG(ret);
	} else if (WIFSTOPPED(ret)) {
		ret = WSTOPSIG(ret);
	} else {
		write_log(2, "Return value %d in %s", ret, __func__);
	}

out:
	return ret;
}

int32_t mount_smart_cache()
{
	char exec_command[400] = {0};
	int32_t ret;

	strcpy(exec_command, "mount -t ext4 /dev/block/loop7 ");
	strcat(exec_command, SMART_CACHE_PATH);
	ret = system(exec_command);
	if (ret < 0) {
		write_log(0, "Fail to exec command in %s. Code %d",
				__func__, errno);
		goto out;
	}

	if (WIFEXITED(ret)) {
		ret = WEXITSTATUS(ret);
	} else if (WIFSIGNALED(ret)) {
		ret = WTERMSIG(ret);
	} else if (WIFSTOPPED(ret)) {
		ret = WSTOPSIG(ret);
	} else {
		write_log(2, "Return value %d in %s", ret, __func__);
	}

out:
	return ret;
}
