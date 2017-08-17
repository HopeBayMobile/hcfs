/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: dir_statistics.c
* Abstract: The c source file for keeping track the number of decendants that
*           are local or cloud or hybrid
*
* Revision History
* 2015/11/11 Jiahong created this file
*
**************************************************************************/

#include "dir_statistics.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "macro.h"
#include "path_reconstruct.h"
#include "parent_lookup.h"
#include "utils.h"

/************************************************************************
*
* Function name: init_dirstat_lookup
*        Inputs: None
*       Summary: Init dir statistics component for HCFS
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t init_dirstat_lookup()
{
	int32_t errcode;
	char pathname[METAPATHLEN+10];

	/* If system is being restored, also need to init a lookup db */
	snprintf(pathname, METAPATHLEN, "%s/dirstat_lookup_db", METAPATH);
	if (access(pathname, F_OK) != 0)
		MKNOD(pathname, S_IFREG | 0600, 0);
	dirstat_lookup_data_fptr = fopen(pathname, "r+");
	if (dirstat_lookup_data_fptr == NULL) {
		errcode = errno;
		write_log(0, "Unexpected error in init: %d (%s)\n", errcode,
			strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	setbuf(dirstat_lookup_data_fptr, NULL);
	return 0;
errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: destroy_dirstat_lookup
*        Inputs: None
*       Summary: Cleanup dir statistics component for HCFS when shutting down
*  Return value: None
*
*************************************************************************/
void destroy_dirstat_lookup()
{
	fclose(dirstat_lookup_data_fptr);
	return;
}

/************************************************************************
*
* Function name: reset_dirstat_lookup
*        Inputs: ino_t thisinode
*       Summary: Reset / init the statistics for an inode
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t reset_dirstat_lookup(ino_t thisinode)
{
	DIR_STATS_TYPE tmpstat;
	off_t filepos;
	int32_t ret;
	int32_t errcode;

	if (thisinode <= 0)
		return -EINVAL;
	memset(&tmpstat, 0, sizeof(DIR_STATS_TYPE));
	ret = sem_wait(&(pathlookup_data_lock));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Unexpected error: %d (%s)\n", errcode,
			strerror(errcode));
		errcode = -errcode;
		return errcode;
	}

	filepos = (off_t) ((thisinode - 1) * sizeof(DIR_STATS_TYPE));
	PWRITE(fileno(dirstat_lookup_data_fptr), &tmpstat,
	       sizeof(DIR_STATS_TYPE), filepos);

	sem_post(&(pathlookup_data_lock));
	return 0;
errcode_handle:
	sem_post(&(pathlookup_data_lock));
	return errcode;
}

/************************************************************************
*
* Function name: update_dirstat_file
*        Inputs: ino_t thisinode, DIR_STATS_TYPE *newstat
*       Summary: Change the dir statistics using the delta specified in
*                "*newstat", for all inodes on the tree path from the
*                parents of "thisinode" to the root.
*                "thisinode" is the file with the location type change.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t update_dirstat_file(ino_t thisinode, DIR_STATS_TYPE *newstat)
{
	DIR_STATS_TYPE tmpstat;
	off_t filepos;
	ino_t current_inode;
	ino_t *parentlist;
	int32_t errcode, ret;
	int32_t numparents, count;
	PRIMARY_PARENT_T tmpparent;

	if (!thisinode)
		return -EINVAL;
	ret = sem_wait(&(pathlookup_data_lock));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Unexpected error: %d (%s)\n", errcode,
		          strerror(errcode));
		errcode = -errcode;
		return errcode;
	}
	parentlist = NULL;
	ret = fetch_all_parents(thisinode, &numparents, &parentlist);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	for (count = 0; count < numparents; count++) {
		current_inode = parentlist[count];
		while (current_inode > 0) {
			/* Update the statistics of the current inode */
			filepos = (off_t) ((current_inode - 1) *
			                    sizeof(DIR_STATS_TYPE));
			PREAD(fileno(dirstat_lookup_data_fptr), &tmpstat,
			       sizeof(DIR_STATS_TYPE), filepos);

			tmpstat.num_local += newstat->num_local;
			tmpstat.num_cloud += newstat->num_cloud;
			tmpstat.num_hybrid += newstat->num_hybrid;
			PWRITE(fileno(dirstat_lookup_data_fptr), &tmpstat,
			       sizeof(DIR_STATS_TYPE), filepos);
			/* Find the parent */
			memset(&tmpparent, 0, sizeof(PRIMARY_PARENT_T));
			filepos = (off_t) ((current_inode - 1) *
						sizeof(PRIMARY_PARENT_T));
			PREAD(fileno(pathlookup_data_fptr), &tmpparent,
			      sizeof(PRIMARY_PARENT_T), filepos);
			current_inode = tmpparent.parentinode;
		}
	}

	free(parentlist);
	parentlist = NULL;
	sem_post(&(pathlookup_data_lock));
	return 0;
errcode_handle:
	if (parentlist != NULL) {
		free(parentlist);
		parentlist = NULL;
	}
	sem_post(&(pathlookup_data_lock));
	return errcode;
}

/************************************************************************
*
* Function name: update_dirstat_parent
*        Inputs: ino_t baseinode, DIR_STATS_TYPE *newstat
*       Summary: Change the dir statistics using the delta specified in
*                "*newstat", for all inodes on the tree path from "baseinode"
*                "baseinode" to the root.
*                pathlookup_data_lock should be locked before calling.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t update_dirstat_parent(ino_t baseinode, DIR_STATS_TYPE *newstat)
{
	DIR_STATS_TYPE tmpstat;
	off_t filepos;
	ino_t current_inode;
	int32_t ret;
	int32_t sem_val;
	PRIMARY_PARENT_T tmpparent;

	if (!baseinode)
		return -EINVAL;
	ret = sem_getvalue(&(pathlookup_data_lock), &sem_val);
	if ((sem_val > 0) || (ret < 0))
		return -EINVAL;

	current_inode = baseinode;
	while (current_inode > 0) {
		/* Update the statistics of the current inode */
		filepos = (off_t) ((current_inode - 1) *
		                    sizeof(DIR_STATS_TYPE));
		PREAD(fileno(dirstat_lookup_data_fptr), &tmpstat,
		       sizeof(DIR_STATS_TYPE), filepos);
		tmpstat.num_local += newstat->num_local;
		tmpstat.num_cloud += newstat->num_cloud;
		tmpstat.num_hybrid += newstat->num_hybrid;
		PWRITE(fileno(dirstat_lookup_data_fptr), &tmpstat,
		       sizeof(DIR_STATS_TYPE), filepos);
		/* Find the parent */
		memset(&tmpparent, 0, sizeof(PRIMARY_PARENT_T));
		filepos = (off_t) ((current_inode - 1) *
					sizeof(PRIMARY_PARENT_T));
		PREAD(fileno(pathlookup_data_fptr), &tmpparent,
		      sizeof(PRIMARY_PARENT_T), filepos);
		current_inode = tmpparent.parentinode;
	}

	return 0;
errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: read_dirstat_lookup
*        Inputs: ino_t thisinode, DIR_STATS_TYPE *newstat
*       Summary: Return the dir statistics for "thisinode" via "newstat"
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t read_dirstat_lookup(ino_t thisinode, DIR_STATS_TYPE *newstat)
{
	DIR_STATS_TYPE tmpstat;
	off_t filepos;
	int32_t ret;
	int32_t errcode;

	if (!thisinode)
		return -EINVAL;
	ret = sem_wait(&(pathlookup_data_lock));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Unexpected error: %d (%s)\n", errcode,
		          strerror(errcode));
		errcode = -errcode;
		return errcode;
	}

	filepos = (off_t) ((thisinode - 1) * sizeof(DIR_STATS_TYPE));
	PREAD(fileno(dirstat_lookup_data_fptr), &tmpstat,
	      sizeof(DIR_STATS_TYPE), filepos);
	memcpy(newstat, &tmpstat, sizeof(DIR_STATS_TYPE));

	sem_post(&(pathlookup_data_lock));
	return 0;
errcode_handle:
	sem_post(&(pathlookup_data_lock));
	return errcode;
}

