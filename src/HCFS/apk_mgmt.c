/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: apk_mgmt.c
* Abstract: The c file to control Android app pin/unpin status.
*
* Revision History
* 2016/12/07 Jethro created this file.
*
**************************************************************************/

#include "apk_mgmt.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>

#include "fuseop.h"
#include "global.h"
#include "hfuse_system.h"
#include "macro.h"
#include "params.h"

int32_t toggle_use_minimal_apk(bool new_val)
{
	int32_t ret = 0;
	bool old_val = hcfs_system->use_minimal_apk;

	if (old_val == false && new_val == true)
		ret = initialize_minimal_apk();
	else if (old_val == true && new_val == false)
		ret = terminate_minimal_apk();

	return ret;
}

int32_t initialize_minimal_apk(void) {
	/* Enable use_minimal_apk after everything ready */
	hcfs_system->use_minimal_apk = true;
	return 0;
}
int32_t terminate_minimal_apk(void) {
	/* Disable use_minimal_apk first, then destroy everything */
	hcfs_system->use_minimal_apk = false;
	return 0;
}


/**
 * Create structure of minimal apk lookup table.
 *
 * @return 0 on success, otherwise negation of error code.
 */
int32_t create_minapk_table()
{
	return 0;
}

/**
 * Destroy structure of minimal apk lookup table.
 *
 * @return 0 on success, otherwise negation of error code.
 */
void destroy_minapk_table()
{
	return;
}

/**
 * Insert triple (parent inode, apk name, miminal apk inode) into lookup table.
 *
 * @param parent_ino Parent inode number.
 * @param apk_name Apk file name.
 * @param minapk_ino Inode number of the minimal apk file.
 *
 * @return 0 on success, -EEXIST if key pair (parent inode, apk name) exists.
 */
int32_t insert_minapk_data(ino_t parent_ino, const char *apk_name,
                           ino_t minapk_ino)
{
	UNUSED(parent_ino);
	UNUSED(apk_name);
	UNUSED(minapk_ino);
	return 0;
}

/**
 * Query the mini apk lookup table by pair (parent inode, apk name) and get the
 * inode # of minimal apk file.
 *
 * @param parent_ino Parent inode number.
 * @param apk_name Apk file name.
 * @param minapk_ino Inode number of the minimal apk file.
 *
 * @return 0 on success, -ENOENT if key pair (parent inode, apk name)
 *         does not exist.
 */
int32_t query_minapk_data(ino_t parent_ino, const char *apk_name,
                          ino_t *minapk_ino)
{
	UNUSED(parent_ino);
	UNUSED(apk_name);
	*minapk_ino = 0;
	return 0;
}

/**
 * Query the mini apk lookup table by pair (parent inode, apk name) and remove
 * the entry from this lookup table.
 *
 * @param parent_ino Parent inode number.
 * @param apk_name Apk file name.
 *
 * @return 0 on success, -ENOENT if key pair (parent inode, apk name)
 *         does not exist.
 */
int32_t remove_minapk_data(ino_t parent_ino, const char *apk_name)
{
	UNUSED(parent_ino);
	UNUSED(apk_name);
	return 0;
}
