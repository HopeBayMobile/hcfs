/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: filetables.c
* Abstract: The c source code file for file table managing.
*
* Revision History
* 2015/2/10 ~ 11 Jiahong added header for this file, and revising coding style.
* 2015/2/11  Jiahong moved "seek_page" and "advance_block" to metaops
*
**************************************************************************/

#include "filetables.h"

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <attr/xattr.h>
#include <sys/mman.h>

#include "fuseop.h"
#include "params.h"
#include "global.h"

/************************************************************************
*
* Function name: init_system_fh_table
*        Inputs: None
*       Summary: Initialize file handle table for the system.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int init_system_fh_table(void)
{
	memset(&system_fh_table, 0, sizeof(FH_TABLE_TYPE));
	/* Init entry_table_flag*/
	system_fh_table.entry_table_flags = malloc(sizeof(char) *
							MAX_OPEN_FILE_ENTRIES);
	if (system_fh_table.entry_table_flags == NULL)
		return -1;
	memset(system_fh_table.entry_table_flags, 0, sizeof(char) *
							MAX_OPEN_FILE_ENTRIES);
	/* Init entry_table*/
	system_fh_table.entry_table = malloc(sizeof(FH_ENTRY) *
							MAX_OPEN_FILE_ENTRIES);
	if (system_fh_table.entry_table == NULL)
		return -1;
	memset(system_fh_table.entry_table, 0, sizeof(FH_ENTRY) *
							MAX_OPEN_FILE_ENTRIES);
	
	system_fh_table.last_available_index = 0;

	sem_init(&(system_fh_table.fh_table_sem), 0, 1);
	return 0;
}

/************************************************************************
*
* Function name: open_fh
*        Inputs: ino_t thisinode, int flags
*       Summary: Allocate a file handle for inode number "thisinode".
*                Also record the file opening flag from "flags".
*  Return value: Index of file handle if successful. Otherwise returns -1.
*
*************************************************************************/
long long open_fh(ino_t thisinode, int flags)
{
	long long index;
	char thismetapath[METAPATHLEN];

	sem_wait(&(system_fh_table.fh_table_sem));

	if (system_fh_table.num_opened_files >= MAX_OPEN_FILE_ENTRIES) {
		sem_post(&(system_fh_table.fh_table_sem));
		/*Not able to allocate any more fh entry as table is full.*/
		return -1;
	}

	index = system_fh_table.last_available_index % MAX_OPEN_FILE_ENTRIES;
	while (system_fh_table.entry_table_flags[index] == TRUE) {
		index++;
		index = index % MAX_OPEN_FILE_ENTRIES;
	}

	system_fh_table.entry_table_flags[index] = TRUE;
	system_fh_table.entry_table[index].meta_cache_ptr = NULL;
	system_fh_table.entry_table[index].meta_cache_locked = FALSE;
	system_fh_table.entry_table[index].thisinode = thisinode;
	system_fh_table.entry_table[index].flags = flags;

	system_fh_table.entry_table[index].blockfptr = NULL;
	system_fh_table.entry_table[index].opened_block = -1;
	system_fh_table.entry_table[index].cached_page_index = -1;
	system_fh_table.entry_table[index].cached_filepos = -1;
	sem_init(&(system_fh_table.entry_table[index].block_sem), 0, 1);

	system_fh_table.num_opened_files++;
	sem_post(&(system_fh_table.fh_table_sem));
	return index;
}

/************************************************************************
*
* Function name: close_fh
*        Inputs: long long index
*       Summary: Close file handle table entry "index".
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int close_fh(long long index)
{
	FH_ENTRY *tmp_entry;

	sem_wait(&(system_fh_table.fh_table_sem));

	tmp_entry = &(system_fh_table.entry_table[index]);
	if (system_fh_table.entry_table_flags[index] == TRUE) {
		if (tmp_entry->meta_cache_locked == FALSE) {
			tmp_entry->meta_cache_ptr =
				meta_cache_lock_entry(tmp_entry->thisinode);
			if (tmp_entry->meta_cache_ptr == NULL) {
				sem_post(&(system_fh_table.fh_table_sem));
				return -1;
			}
			tmp_entry->meta_cache_locked = TRUE;
		}
		meta_cache_close_file(tmp_entry->meta_cache_ptr);

		tmp_entry->meta_cache_locked = FALSE;
		meta_cache_unlock_entry(tmp_entry->meta_cache_ptr);

		system_fh_table.entry_table_flags[index] = FALSE;
		tmp_entry->thisinode = 0;

		if ((tmp_entry->blockfptr != NULL) &&
				(tmp_entry->opened_block >= 0))
			fclose(tmp_entry->blockfptr);

		tmp_entry->meta_cache_ptr = NULL;
		tmp_entry->blockfptr = NULL;
		tmp_entry->opened_block = -1;
		sem_destroy(&(tmp_entry->block_sem));
		system_fh_table.last_available_index = index;
	} else {
		sem_post(&(system_fh_table.fh_table_sem));
		return -1;
	}
	system_fh_table.num_opened_files--;
	sem_post(&(system_fh_table.fh_table_sem));
	return 0;
}

