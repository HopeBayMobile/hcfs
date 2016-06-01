/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: rebuild_parent_dirstat.h
* Abstract: The c source file for rebuilding parent lookup dbs and dir
*           statistics
*
* Revision History
* 2016/6/1 Jiahong created this file
*
**************************************************************************/

#include "rebuild_parent_dirstat.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "parent_lookup.h"
#include "dir_statistics.h"
#include "fuseop.h"
#include "logger.h"
#include "macro.h"
#include "path_reconstruct.h"
#include "utils.h"

static inline int32_t _update_stats(ino_t p_inode, DIR_STATS_TYPE *tmpstat_in)
{
	off_t filepos;
	ino_t current_inode;
	int32_t errcode;
	ssize_t ret_ssize;
	PRIMARY_PARENT_T tmpparent;
	DIR_STATS_TYPE tmpstat;

	current_inode = p_inode;
	while (current_inode > 0) {
		/* Update the statistics of the current inode */
		filepos = (off_t) ((current_inode - 1) *
		                    sizeof(DIR_STATS_TYPE));
		PREAD(fileno(dirstat_lookup_data_fptr), &tmpstat,
		      sizeof(DIR_STATS_TYPE), filepos);

		tmpstat.num_local += tmpstat_in->num_local;
		tmpstat.num_cloud += tmpstat_in->num_cloud;
		tmpstat.num_hybrid += tmpstat_in->num_hybrid;
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

/**********************************************************************//**
*
* Rebuilds parent lookup db for a pair of inodes, and also adds the dir
* statistics related to the pair to dir statistics db.
*
* @param this_inode the child inode of the pair to be rebuilt.
* @param p_inode the parent inode of the pair to be rebuilt.
* @param d_type the filesystem object type of this_inode.
* @return 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t rebuild_parent_stat(ino_t this_inode, ino_t p_inode, int8_t d_type)
{
	ino_t *parent_list;
	int32_t ret, errcode;
	int32_t num_parents, count;
	DIR_STATS_TYPE tmpstat;
	off_t filepos;
	char metapath[METAPATHLEN];
	FILE *metafptr;
	FILE_STATS_TYPE meta_stats;
	DIR_STATS_TYPE tmp_dirstat;
	ssize_t ret_ssize;
	size_t ret_size;

	metafptr = NULL;
	if ((this_inode <= 0) || (p_inode <= 0))
		return -EINVAL;

	/* If system is not being restored, don't do anything */
	if (hcfs_system->system_restoring == FALSE)
		return 0;

	sem_wait(&(pathlookup_data_lock));
	parent_list = NULL;
	num_parents = 0;
	ret = fetch_all_parents(this_inode, &num_parents, &parent_list);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	for (count = 0; count < num_parents; count++)
		if (p_inode == parent_list[count])
			break;
	if (count < num_parents) {
		/* Parent exists in the list already. Do nothing */
		free(parent_list);
		sem_post(&(pathlookup_data_lock));
		return 0;
	}

	if (parent_list != NULL) {
		free(parent_list);
		parent_list = NULL;
	}

	/* Add the parent to the db */
	ret = lookup_add_parent(this_inode, p_inode);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	/* If not a directory nor a regular file, do nothing */
	if ((d_type != D_ISREG) && (d_type != D_ISDIR)) {
		sem_post(&(pathlookup_data_lock));
		return 0;
	}
	/* If the object is a directory, set the dir statistics to zero */
	if (d_type == D_ISDIR) {
		memset(&tmpstat, 0, sizeof(DIR_STATS_TYPE));

		filepos = (off_t) ((this_inode - 1) * sizeof(DIR_STATS_TYPE));
		PWRITE(fileno(dirstat_lookup_data_fptr), &tmpstat,
		       sizeof(DIR_STATS_TYPE), filepos);
		sem_post(&(pathlookup_data_lock));
		return 0;
	}

	/* Check if local meta exists */
	ret = fetch_meta_path(metapath, this_inode);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	if (access(metapath, F_OK) == 0) {
		/* First open and lock the meta file */
		metafptr = fopen(metapath, "r");
		if (metafptr == NULL) {
			errcode = errno;
			write_log(0, "Error when opening meta. (%s)\n",
			          strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		ret = flock(fileno(metafptr), LOCK_EX);
		if (ret < 0) {
			errcode = errno;
			write_log(0, "Error when locking meta. (%s)\n",
			          strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		/* Translate info of blocks to file location */
		FSEEK(metafptr, sizeof(struct stat) + sizeof(FILE_META_TYPE),
		      SEEK_SET);
		FREAD(&meta_stats, sizeof(FILE_STATS_TYPE), 1, metafptr);
		memset(&tmp_dirstat, 0, sizeof(DIR_STATS_TYPE));
		if ((meta_stats.num_blocks == 0) ||
		    	(meta_stats.num_blocks ==
		         meta_stats.num_cached_blocks)) {
			/* If local */
			tmp_dirstat.num_local = 1;
		} else if (meta_stats.num_cached_blocks == 0) {
			/* If cloud */
			tmp_dirstat.num_cloud = 1;
		} else {
			tmp_dirstat.num_hybrid = 1;
		}
		flock(fileno(metafptr), LOCK_UN);
		fclose(metafptr);
		metafptr = NULL;
	} else {
		/* Meta file not found, set location to cloud */
		memset(&tmp_dirstat, 0, sizeof(DIR_STATS_TYPE));
		tmp_dirstat.num_cloud = 1;
	}

	/* Have the location of the file. Now update from the parent
	p_inode to the root */

	ret = _update_stats(p_inode, &tmp_dirstat);
	sem_post(&(pathlookup_data_lock));
	return 0;
errcode_handle:
	if (metafptr != NULL) {
		flock(fileno(metafptr), LOCK_UN);
		fclose(metafptr);
	}
	if (parent_list != NULL)
		free(parent_list);
	sem_post(&(pathlookup_data_lock));
	return errcode;
}
