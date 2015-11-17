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

extern SYSTEM_CONF_STRUCT system_config;

/************************************************************************
*
* Function name: init_dirstat_lookup
*        Inputs: None
*       Summary: Init dir statistics component for HCFS
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int init_dirstat_lookup()
{
	int ret, errcode;
	char pathname[METAPATHLEN+10];

	snprintf(pathname, METAPATHLEN, "%s/dirstat_lookup_db", METAPATH);
	if (access(pathname, F_OK) != 0) {
		MKNOD(pathname, S_IFREG | 0600, 0)
	}
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
int reset_dirstat_lookup(ino_t thisinode)
{
	DIR_STATS_TYPE tmpstat;
	off_t filepos;
	int errcode, ret;
	ssize_t ret_ssize;

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
int update_dirstat_file(ino_t thisinode, DIR_STATS_TYPE *newstat)
{
	DIR_STATS_TYPE tmpstat;
	off_t filepos;
	ino_t current_inode, parent_inode;
	ino_t *parentlist;
	int errcode, ret;
	int numparents, count;
	ssize_t ret_ssize;

	if (thisinode <= 0)
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
			filepos = (off_t) ((current_inode - 1) * sizeof(ino_t));
			PREAD(fileno(pathlookup_data_fptr), &parent_inode,
			      sizeof(ino_t), filepos);
			current_inode = parent_inode;
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
int update_dirstat_parent(ino_t baseinode, DIR_STATS_TYPE *newstat)
{
	DIR_STATS_TYPE tmpstat;
	off_t filepos;
	ino_t current_inode, parent_inode;
	int errcode, ret;
	ssize_t ret_ssize;
	int sem_val;

	if (baseinode <= 0)
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
		filepos = (off_t) ((current_inode - 1) * sizeof(ino_t));
		PREAD(fileno(pathlookup_data_fptr), &parent_inode,
		      sizeof(ino_t), filepos);
		current_inode = parent_inode;
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
int read_dirstat_lookup(ino_t thisinode, DIR_STATS_TYPE *newstat)
{
	DIR_STATS_TYPE tmpstat;
	off_t filepos;
	int errcode, ret;
	ssize_t ret_ssize;

	if (thisinode <= 0)
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

