/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: do_restoration.h
* Abstract: The header file for restore operations
*
* Revision History
* 2016/7/25 Jiahong created this file.
*
**************************************************************************/

#ifndef GW20_DO_RESTORATION_H_
#define GW20_DO_RESTORATION_H_

char restore_metapath[METAPATHLEN];
char restore_blockpath[BLOCKPATHLEN];

#define RESTORE_METAPATH restore_metapath
#define RESTORE_BLOCKPATH restore_blockpath

void init_restore_path(void);

/* Returns path to status file on system restoring */
int32_t fetch_restore_stat_path(char *pathname);

int32_t initiate_restoration();

#endif  /* GW20_DO_RESTORATION_H_ */
