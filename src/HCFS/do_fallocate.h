/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: do_fallocate.h
* Abstract: The c header file for fallocate operation.
*
* Revision History
* 2016/2/23 Jiahong created this file.
*
**************************************************************************/

#ifndef GW20_HCFS_DO_FALLOCATE_H_
#define GW20_HCFS_DO_FALLOCATE_H_

#include <sys/types.h>
#include <fuse/fuse_lowlevel.h>

#include "meta_mem_cache.h"

int32_t do_fallocate(ino_t this_inode,
		     HCFS_STAT *newstat,
		     int32_t mode,
		     off_t offset,
		     off_t length,
		     META_CACHE_ENTRY_STRUCT **body_ptr,
		     fuse_req_t req);

#endif  /* GW20_HCFS_DO_FALLOCATE_H_ */
