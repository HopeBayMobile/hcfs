/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: do_fallocate.c
* Abstract: The c source code file for fallocate operation.
*
* Revision History
* 2016/2/23 Jiahong created this file and moved do_fallocate here.
*
**************************************************************************/

#include "do_fallocate.h"

#include <errno.h>

#include "mount_manager.h"
#include "utils.h"
#include "logger.h"
#include "fuseop.h"
#include "hcfs_cacheops.h"

/* TODO: Implement mode 1 and 3 if needed or in the future
	if mode = 0, default op. (If offset+ length is smaller than org size,
		do nothing)
	if mode = 1, keep filesize.
	if mode = 3, punch hole and keep size.
	otherwise return ENOTSUP
	If mode 0 or 1, don't need to zero regions already containing data
	If mode = 3, need to zero regions.
*/
static int32_t do_fallocate_extend(ino_t this_inode,
				   HCFS_STAT *filestat,
				   off_t offset,
				   META_CACHE_ENTRY_STRUCT **body_ptr,
				   fuse_req_t req)
{
	FILE_META_TYPE tempfilemeta;
	int32_t ret;
	int64_t sizediff, pin_sizediff;
	int64_t max_pinned_size = 0;
	MOUNT_T *tmpptr;
	long avail_space, avail_space1 = __LONG_MAX__, avail_space2;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	write_log(10, "Debug fallocate mode 0: offset %ld\n", offset);
	/* If the filesystem object is not a regular file, return error */
	if (S_ISREG(filestat->mode) == FALSE) {
		if (S_ISDIR(filestat->mode))
			return -EISDIR;
		else
			return -EPERM;
	}

	ret = meta_cache_lookup_file_data(this_inode, NULL, &tempfilemeta,
			NULL, 0, *body_ptr);

	if (ret < 0)
		return ret;

	if (filestat->size >= offset) {
		/*Do nothing if no change needed */
		write_log(10,
			"Debug fallocate mode 0: Nothing changed.\n");
		return 0;
	}

	if (filestat->size < offset) {
		sizediff = (int64_t) offset - filestat->size;
		pin_sizediff = round_size((int64_t)offset) -
				round_size(filestat->size);
		sem_wait(&(hcfs_system->access_sem));
		/* Check system size and reject if exceeding quota */
		if (hcfs_system->systemdata.system_size + sizediff >
				hcfs_system->systemdata.system_quota) {
			sem_post(&(hcfs_system->access_sem));
			notify_avail_space(0);
			return -ENOSPC;
		}
		/* If this is a pinned file and we want to extend the file,
		need to find out if pinned space is still available for this
		extension */
		/* If pinned space is available, add the amount of changes
		to the total usage first */
		if (P_IS_PIN(tempfilemeta.local_pin)) {
			max_pinned_size =
				get_pinned_limit(tempfilemeta.local_pin);
			if (max_pinned_size < 0) {
				sem_post(&(hcfs_system->access_sem));
				return -EINVAL;
			}
			if ((hcfs_system->systemdata.pinned_size + pin_sizediff)
					> max_pinned_size) {
				sem_post(&(hcfs_system->access_sem));
				notify_avail_space(0);
				return -ENOSPC;
			}
			hcfs_system->systemdata.pinned_size += pin_sizediff;
		}
		sem_post(&(hcfs_system->access_sem));
	}

	/* Update file and system meta here */
	change_system_meta((int64_t)(offset - filestat->size),
			0, 0, 0, 0, 0, TRUE);

	ret = change_mount_stat(tmpptr,
			(int64_t) (offset - filestat->size), 0, 0);
	if (ret < 0)
		return ret;

	filestat->size = offset;
	filestat->blocks = (filestat->size + 511) / 512;
	filestat->mtime = time(NULL);

	if (max_pinned_size)
		avail_space1 =
		    max_pinned_size - hcfs_system->systemdata.pinned_size;
	avail_space2 =
	    hcfs_system->systemdata.system_quota -
	    hcfs_system->systemdata.system_size;
	avail_space = avail_space1 < avail_space2 ? avail_space1 : avail_space2;
	notify_avail_space(avail_space - WRITEBACK_CACHE_RESERVE_SPACE);

	return 0;
}

int32_t do_fallocate(ino_t this_inode,
		     HCFS_STAT *newstat,
		     int32_t mode,
		     off_t offset,
		     off_t length,
		     META_CACHE_ENTRY_STRUCT **body_ptr,
		     fuse_req_t req)
{
	int32_t ret_val;

	switch (mode) {
	case 0:
		ret_val = do_fallocate_extend(this_inode, newstat,
		                              offset + length, body_ptr, req);
		break;
/* mode 1 and 3 are not implemented at this time
	case 1:
		ret_val = do_fallocate_keepsize(this_inode, newstat,
		                                offset, length, body_ptr, req);
		break;
	case 3:
		ret_val = do_fallocate_punchhole(this_inode, newstat,
						offset, length, body_ptr, req);
		break;
*/
	default:
		ret_val = -ENOTSUP;
		break;
	}

	return ret_val;
}

