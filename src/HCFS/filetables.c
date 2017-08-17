/*************************************************************************
*
* Copyright © 2014-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: filetables.c
* Abstract: The c source code file for file table managing.
*
* Revision History
* 2015/2/10 ~ 11 Jiahong added header for this file, and revising coding style.
* 2015/2/11  Jiahong moved "seek_page" and "advance_block" to metaops
* 2015/6/2  Jiahong added error handling
* 2016/4/20 Jiahong adding operations for dir handle table
* 2016/5/20 Jiahong fixing a hang issue due to meta handling in close_fh
*
**************************************************************************/

#include "filetables.h"

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/file.h>

#include "fuseop.h"
#include "params.h"
#include "global.h"
#include "macro.h"
#include "utils.h"

/************************************************************************
*
* Function name: init_system_fh_table
*        Inputs: None
*       Summary: Initialize file handle table for the system.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t init_system_fh_table(void)
{
	memset(&system_fh_table, 0, sizeof(FH_TABLE_TYPE));
	/* Init entry_table_flag*/
	system_fh_table.entry_table_flags = malloc(sizeof(uint8_t) *
							MAX_OPEN_FILE_ENTRIES);
	if (system_fh_table.entry_table_flags == NULL)
		return -ENOMEM;
	memset(system_fh_table.entry_table_flags, 0, sizeof(uint8_t) *
							MAX_OPEN_FILE_ENTRIES);
	/* Init entry_table*/
	system_fh_table.entry_table = malloc(sizeof(FH_ENTRY) *
							MAX_OPEN_FILE_ENTRIES);
	if (system_fh_table.entry_table == NULL) {
		free(system_fh_table.entry_table_flags);
		return -ENOMEM;
	}
	memset(system_fh_table.entry_table, 0, sizeof(FH_ENTRY) *
							MAX_OPEN_FILE_ENTRIES);

	/* Init dir entry_table*/
	system_fh_table.direntry_table = malloc(sizeof(DIRH_ENTRY) *
							MAX_OPEN_FILE_ENTRIES);
	if (system_fh_table.direntry_table == NULL) {
		free(system_fh_table.entry_table_flags);
		free(system_fh_table.entry_table);
		return -ENOMEM;
	}
	memset(system_fh_table.direntry_table, 0, sizeof(DIRH_ENTRY) *
							MAX_OPEN_FILE_ENTRIES);

	system_fh_table.last_available_index = 0;

	sem_init(&(system_fh_table.fh_table_sem), 0, 1);
	return 0;
}

/************************************************************************
*
* Function name: open_fh
*        Inputs: ino_t thisinode, int32_t flags, BOOL isdir
*       Summary: Allocate a file handle for inode number "thisinode".
*                Also record the file opening flag from "flags".
*                If "isdir" is true, the handle to open is a directory
*                instead of a regular file.
*  Return value: Index of file handle if successful. Otherwise returns
*                negation of error code.
*
*************************************************************************/
int64_t open_fh(ino_t thisinode, int32_t flags, BOOL isdir)
{
	int64_t index;
	DIRH_ENTRY *dirh_ptr;

	sem_wait(&(system_fh_table.fh_table_sem));

	if (system_fh_table.num_opened_files >= MAX_OPEN_FILE_ENTRIES) {
		sem_post(&(system_fh_table.fh_table_sem));
		/*Not able to allocate any more fh entry as table is full.*/
		return -EMFILE;
	}

	index = system_fh_table.last_available_index % MAX_OPEN_FILE_ENTRIES;
	while (system_fh_table.entry_table_flags[index] != NO_FH) {
		index++;
		index = index % MAX_OPEN_FILE_ENTRIES;
	}

	if (isdir) {
		system_fh_table.entry_table_flags[index] = IS_DIRH;
		dirh_ptr = &(system_fh_table.direntry_table[index]);
		dirh_ptr->thisinode = thisinode;
		dirh_ptr->flags = flags;
		dirh_ptr->snapshot_ptr = NULL;
		sem_init(&(dirh_ptr->snap_ref_sem), 0, 0);
		sem_init(&(dirh_ptr->wait_ref_sem), 0, 1);
		system_fh_table.have_nonsnap_dir = TRUE;
	} else {
		system_fh_table.entry_table_flags[index] = IS_FH;
		system_fh_table.entry_table[index].meta_cache_ptr = NULL;
		system_fh_table.entry_table[index].meta_cache_locked = FALSE;
		system_fh_table.entry_table[index].thisinode = thisinode;
		system_fh_table.entry_table[index].flags = flags;

		system_fh_table.entry_table[index].blockfptr = NULL;
		system_fh_table.entry_table[index].opened_block = -1;
		system_fh_table.entry_table[index].cached_page_index = -1;
		system_fh_table.entry_table[index].cached_filepos = -1;
		sem_init(&(system_fh_table.entry_table[index].block_sem), 0, 1);
	}

	system_fh_table.num_opened_files++;
	sem_post(&(system_fh_table.fh_table_sem));
	return index;
}

/************************************************************************
*
* Function name: close_fh
*        Inputs: int64_t index
*       Summary: Close file handle table entry "index".
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int32_t close_fh(int64_t index)
{
	FH_ENTRY *tmp_entry;
	DIRH_ENTRY *tmp_DIRH_entry;
	META_CACHE_ENTRY_STRUCT *tmp_ptr;
	BOOL need_close_cache, need_open_cache;
	ino_t tmpinode;

	need_close_cache = FALSE;
	need_open_cache = FALSE;
	tmp_ptr = NULL;
	tmpinode = 0;

	sem_wait(&(system_fh_table.fh_table_sem));

	if (system_fh_table.entry_table_flags[index] == IS_FH) {
		tmp_entry = &(system_fh_table.entry_table[index]);
		tmpinode = tmp_entry->thisinode;
		need_close_cache = TRUE;
		if (tmp_entry->meta_cache_locked == TRUE) {
			tmp_ptr = tmp_entry->meta_cache_ptr;
		} else {
			need_open_cache = TRUE;
			tmpinode = tmp_entry->thisinode;
		}

		tmp_entry->meta_cache_locked = FALSE;

		system_fh_table.entry_table_flags[index] = NO_FH;
		tmp_entry->thisinode = 0;

		if ((tmp_entry->blockfptr != NULL) &&
				(tmp_entry->opened_block >= 0))
			fclose(tmp_entry->blockfptr);

		tmp_entry->meta_cache_ptr = NULL;
		tmp_entry->blockfptr = NULL;
		tmp_entry->opened_block = -1;
		sem_destroy(&(tmp_entry->block_sem));
		system_fh_table.last_available_index = index;
	} else if (system_fh_table.entry_table_flags[index] == IS_DIRH) {
		tmp_DIRH_entry = &(system_fh_table.direntry_table[index]);

		if (tmp_DIRH_entry->snapshot_ptr != NULL) {
			fclose(tmp_DIRH_entry->snapshot_ptr);
			tmp_DIRH_entry->snapshot_ptr = NULL;
		}
		system_fh_table.entry_table_flags[index] = NO_FH;
		tmp_DIRH_entry->thisinode = 0;
		sem_destroy(&(tmp_DIRH_entry->snap_ref_sem));
		sem_destroy(&(tmp_DIRH_entry->wait_ref_sem));

		system_fh_table.last_available_index = index;
	} else {
		sem_post(&(system_fh_table.fh_table_sem));
		return -1;
	}
	system_fh_table.num_opened_files--;
	sem_post(&(system_fh_table.fh_table_sem));
	if (need_open_cache == TRUE) {
		tmp_ptr = meta_cache_lock_entry(tmpinode);
		if (tmp_ptr == NULL)
			return 0;
	}
	if (need_close_cache == TRUE) {
		meta_cache_close_file(tmp_ptr);
		meta_cache_unlock_entry(tmp_ptr);
	}
	return 0;
}

/************************************************************************
*
* Function name: handle_dirmeta_snapshot
*        Inputs: ino_t thisinode, FILE *metafptr
*       Summary: Scans filetable and create snapshot of meta for inode
*                "thisinode". "metafptr" points to the file stream
*                of the source meta file. "metafptr" needs to be locked.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t handle_dirmeta_snapshot(ino_t thisinode, FILE *metafptr)
{
	int count;
	DIRH_ENTRY *tmp_entry;
	BOOL snap_created;
	char snap_name[METAPATHLEN + 1];
	int64_t ret_pos;
	int32_t errcode;
	FILE *snapfptr = NULL;
	size_t ret_size;
	uint8_t buf[4096];
	BOOL have_opened_nonsnap;

	snap_created = FALSE;
	if (metafptr == NULL)
		return -EIO;

	sem_wait(&(system_fh_table.fh_table_sem));
	if (system_fh_table.have_nonsnap_dir == FALSE) {
		sem_post(&(system_fh_table.fh_table_sem));
		return 0;
	}

	have_opened_nonsnap = FALSE;
	snprintf(snap_name, METAPATHLEN, "%s/tmp_dirmeta_snap", METAPATH);

	for (count = 0; count < MAX_OPEN_FILE_ENTRIES; count++) {
		tmp_entry = &(system_fh_table.direntry_table[count]);
		if ((system_fh_table.entry_table_flags[count] == IS_DIRH) &&
		    (tmp_entry->snapshot_ptr == NULL)) {
			if ((tmp_entry->thisinode == thisinode) &&
			    (snap_created == FALSE)) {
				/* Create snapshot */
				if (access(snap_name, F_OK) == 0)
					UNLINK(snap_name);
				ret_pos = FTELL(metafptr);
				snapfptr = fopen(snap_name, "w");
				if (snapfptr == NULL) {
					errcode = -errno;
					write_log(0, "Error in meta snap\n");
					write_log(0, "Code %d\n", errcode);
					errcode = -EIO;
					goto errcode_handle;
				}
				FSEEK(metafptr, 0, SEEK_SET);
				while (!feof(metafptr)) {
					ret_size = FREAD(buf, 1, sizeof(buf), metafptr);
					if (ret_size == 0)
						break;
					FWRITE(buf, 1, ret_size, snapfptr);
				}
				FSEEK(metafptr, ret_pos, SEEK_SET);
				fclose(snapfptr);
				snap_created = TRUE;
			}
			if (tmp_entry->thisinode == thisinode) {
				tmp_entry->snapshot_ptr = fopen(snap_name, "r");
				if (tmp_entry->snapshot_ptr == NULL) {
					errcode = -errno;
					write_log(0, "Error in meta snap\n");
					write_log(0, "Code %d\n", errcode);
					errcode = -EIO;
					goto errcode_handle;
				}
				sem_destroy(&(tmp_entry->snap_ref_sem));
				sem_destroy(&(tmp_entry->wait_ref_sem));
				sem_init(&(tmp_entry->snap_ref_sem), 0, 0);
				sem_init(&(tmp_entry->wait_ref_sem), 0, 1);
			} else {
				have_opened_nonsnap = TRUE;
			}
		}
	}

	if (have_opened_nonsnap)
		system_fh_table.have_nonsnap_dir = TRUE;
	else
		system_fh_table.have_nonsnap_dir = FALSE;

	if (snap_created)
		unlink(snap_name);
	sem_post(&(system_fh_table.fh_table_sem));
	return 0;
errcode_handle:
	if (snap_created)
		unlink(snap_name);
	sem_post(&(system_fh_table.fh_table_sem));
	if (snapfptr)
		fclose(snapfptr);
	return errcode;
}
