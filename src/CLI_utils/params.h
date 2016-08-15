/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: params.h
* Abstract: The header file for handling system parameters for HCFS
*
* Revision History
* 2015/2/11 Jiahong added header for this file and revised coding style.
*
**************************************************************************/

#ifndef GW20_SRC_PARAMS_H_
#define GW20_SRC_PARAMS_H_
#include <inttypes.h>

#define MAX_PINNED_RATIO 0.8
#define MAX_PINNED_LIMIT (CACHE_HARD_LIMIT * MAX_PINNED_RATIO)

#define MAX_META_MEM_CACHE_ENTRIES 5000
#define NUM_META_MEM_CACHE_HEADERS 5000
#define META_CACHE_FLUSH_NOW TRUE

#define METAPATHLEN 400
#define BLOCKPATHLEN 400
#define MAX_FILENAME_LEN 255
#define MONITOR_TEST_TIMEOUT 10
#define MONITOR_BACKOFF_SLOT 1
#define MONITOR_MAX_BACKOFF_EXPONENT 9

#define NUMSUBDIR 1000

#define RECLAIM_TRIGGER 10000

#define NO_LL 0
#define IS_DIRTY 1
#define TO_BE_DELETED 2
#define TO_BE_RECLAIMED 3
#define RECLAIMED 4

#define XFER_WINDOW_MAX 6
#define XFER_WINDOW_SIZE 3
#define XFER_SEC_PER_WINDOW 20
#define XFER_SLOW_SPEED 32 /* in KB/s */

static const char DEFAULT_CONFIG_PATH[] = "/data/hcfs.conf";
static const char CONFIG_PASSPHRASE[] = "lets encrypt configuration";
static const char USERMETA_PASSPHRASE[] = "hey! kewei enc usermeta :)";

#endif  /* GW20_SRC_PARAMS_H_ */
