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

/* TODO: Implement mode 1 and 3 if needed or in the future
	if mode = 0, default op. (If offset+ length is smaller than org size,
		do nothing)
	if mode = 1, keep filesize.
	if mode = 3, punch hole and keep size.
	otherwise return ENOTSUP
	If mode 0 or 1, don't need to zero regions already containing data
	If mode = 3, need to zero regions.
*/
static int do_fallocate_extend(ino_t this_inode, struct stat *filestat,
		off_t offset, META_CACHE_ENTRY_STRUCT **body_ptr,
		fuse_req_t req)
{
	FILE_META_TYPE tempfilemeta;
	int ret, errcode;
	long long sizediff;
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	write_log(10, "Debug fallocate mode 0: offset %ld\n", offset);
	/* If the filesystem object is not a regular file, return error */
	if (S_ISREG(filestat->st_mode) == FALSE) {
		if (S_ISDIR(filestat->st_mode))
			return -EISDIR;
		else
			return -EPERM;
	}

	ret = meta_cache_lookup_file_data(this_inode, NULL, &tempfilemeta,
			NULL, 0, *body_ptr);

	if (ret < 0)
		return ret;

	if (filestat->st_size >= offset) {
		/*Do nothing if no change needed */
		write_log(10,
			"Debug fallocate mode 0: Nothing changed.\n");
		return 0;
	}

	/* TODO: will need to allocate space in quota as well */
	if ((tempfilemeta.local_pin == TRUE) &&
	    (filestat->st_size < offset)) {
		/* If this is a pinned file and we want to extend the file,
		need to find out if pinned space is still available for this
		extension */
		/* If pinned space is available, add the amount of changes
		to the total usage first */
		sizediff = (long long) offset - filestat->st_size;
		sem_wait(&(hcfs_system->access_sem));
		if ((hcfs_system->systemdata.pinned_size + sizediff)
			> MAX_PINNED_LIMIT) {
			sem_post(&(hcfs_system->access_sem));
			return -ENOSPC;
		}
		hcfs_system->systemdata.pinned_size += sizediff;
		sem_post(&(hcfs_system->access_sem));
	}

	ret = meta_cache_update_file_data(this_inode, filestat,
			&tempfilemeta, NULL, 0, *body_ptr);
	if (ret < 0) {
		write_log(0, "IO error in fallocate. Data may ");
		write_log(0, "not be consistent\n");
		errcode = ret;
		goto errcode_handle;
	}

	if ((tempfilemeta.local_pin == TRUE) &&
	    (offset < filestat->st_size)) {
		sem_wait(&(hcfs_system->access_sem));
		hcfs_system->systemdata.pinned_size +=
			(long long)(offset - filestat->st_size);
		if (hcfs_system->systemdata.pinned_size < 0)
			hcfs_system->systemdata.pinned_size = 0;
		sem_post(&(hcfs_system->access_sem));
	}

	/* Update file and system meta here */
	change_system_meta((long long)(offset - filestat->st_size), 0, 0, 0);

	ret = change_mount_stat(tmpptr,
			(long long) (offset - filestat->st_size), 0);
	if (ret < 0)
		return ret;

	filestat->st_size = offset;
	filestat->st_mtime = time(NULL);

	return 0;
errcode_handle:
	/* If an error occurs, need to revert the changes to pinned size */
	if ((tempfilemeta.local_pin == TRUE) &&
	    (offset > filestat->st_size)) {
		sem_wait(&(hcfs_system->access_sem));
		hcfs_system->systemdata.pinned_size -=
			(long long)(offset - filestat->st_size);
		if (hcfs_system->systemdata.pinned_size < 0)
			hcfs_system->systemdata.pinned_size = 0;
		sem_post(&(hcfs_system->access_sem));
	}

	return errcode;

}
int do_fallocate(ino_t this_inode, struct stat *newstat, int mode,
		off_t offset, off_t length,
		META_CACHE_ENTRY_STRUCT **body_ptr, fuse_req_t req)
{
	int ret_val;

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

