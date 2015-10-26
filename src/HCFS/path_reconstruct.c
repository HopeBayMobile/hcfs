/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: path_reconstruct.c
* Abstract: The c source file for reconstructing path from parent lookups
*
* Revision History
* 2015/10/26 Jiahong created this file
*
**************************************************************************/

#include "path_reconstruct.h"

#include <errno.h>
#include <string.h>
#include <semaphore.h>

#include "logger.h"
/************************************************************************
*
* Function name: init_pathcache
*        Inputs: None
*       Summary: Init cache for inode to path lookup
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int init_pathcache()
{
	int ret, errcode;

	ret = sem_init(&pathcache_lock, 0, 1);
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Error: %d, %s\n", errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	memset(pathcache, 0, sizeof(PATH_HEAD_ENTRY) * NUM_LOOKUP_ENTRY);

	return 0;
errcode_handle:
	return errcode;
}
/************************************************************************
*
* Function name: destroy_pathcache
*        Inputs: None
*       Summary: Free up cache for inode to path lookup
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int destroy_pathcache()
{

}

int construct_path(ino_t thisinode, char **result, int bufsize);
int lookup_name(ino_t thisinode, char *namebuf);


#endif  /* GW20_HCFS_PATH_RECONSTRUCT_H_ */

