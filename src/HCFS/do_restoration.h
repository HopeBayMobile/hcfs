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

#include <inttypes.h>
#include <semaphore.h>

#include "global.h"
#include "params.h"

char restore_metapath[METAPATHLEN];
char restore_blockpath[BLOCKPATHLEN];

sem_t restore_sem;

#define RESTORE_METAPATH restore_metapath
#define RESTORE_BLOCKPATH restore_blockpath

void init_restore_path(void);

/* Returns path to status file on system restoring */
int32_t fetch_restore_stat_path(char *pathname);

int32_t tag_restoration(char *content);

int32_t initiate_restoration(void);

int32_t check_restoration_status(void);

#endif  /* GW20_DO_RESTORATION_H_ */
