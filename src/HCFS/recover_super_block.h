/*************************************************************************
 * *
 * * Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.v
 * *
 * * File Name: recover_super_block.h
 * * Abstract: The c header file for recovering queues in super block.
 * *
 * * Revision History
 * * 2016/10/24 Yuxun created this file.
 * **************************************************************************/
#ifndef GW20_HCFS_RECOVER_SB_H_
#define GW20_HCFS_RECOVER_SB_H_

#include "global.h"
#include "super_block.h"

#define PROGRESS_FILE "recover_dirty_in_progress"
#define MAX_NUM_ENTRY_HANDLE 512
#define MIN_RECOVERY_INTERVAL 3600 /* Only trigger recovery once in this interval */

typedef struct recovery_round_data {
	/* Last dirty entry in previous reconstruct round */
	SUPER_BLOCK_ENTRY prev_last_entry;
	/* First dirty entry in this round */
	SUPER_BLOCK_ENTRY this_first_entry;
	/* Last dirty entry in this round */
	SUPER_BLOCK_ENTRY this_last_entry;
	/* Total dirty entries processed this round */
	int64_t this_num_dirty;
	int64_t dirty_size_delta;
	int64_t unpin_dirty_size_delta;
} RECOVERY_ROUND_DATA;

BOOL need_recover_sb();
void fetch_recover_progressf_path(char *pathname);
void *recover_sb_queue_worker(void *ptr);

#endif /* GW20_HCFS_RECOVER_SB_H_ */
