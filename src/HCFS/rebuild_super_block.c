/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "rebuild_super_block.h"

#include <unistd.h>
#include <pthread.h>
#include <sys/file.h>

#include "logger.h"
#include "errno.h"
#include "params.h"
#include "utils.h"
#include "metaops.h"
#include "macro.h"
#include "global.h"
#include "hcfs_fromcloud.h"
#include "hfuse_system.h"
#include "rebuild_parent_dirstat.h"
#include "do_restoration.h"
#include "parent_lookup.h"

#define LOCK_QUEUE_FILE() sem_wait(&(rebuild_sb_jobs->queue_file_sem))
#define UNLOCK_QUEUE_FILE() sem_post(&(rebuild_sb_jobs->queue_file_sem))

/**
 * Helper of init_rebuild_sb(). Help the caller to get all root inodes
 * from object "FSmgr" on cloud. Download the object if "FSmgr" does
 * not exist in local device.
 */
int32_t _get_root_inodes(ino_t **roots, int64_t *num_inodes)
{
	char fsmgr_path[METAPATHLEN], objname[200];
	FILE *fsmgr_fptr;
	DIR_META_TYPE dirmeta;
	int32_t errcode, ret;
	int64_t num_dir, num_nondir;
	ino_t *dir_nodes, *nondir_nodes;

	*roots = NULL;
	*num_inodes = 0;
	sprintf(fsmgr_path, "%s/fsmgr", METAPATH);
	if (access(fsmgr_path, F_OK) < 0) {
		errcode = errno;
		if (errcode == ENOENT) {
			/* Get fsmgr from cloud */
			sprintf(objname, FSMGR_BACKUP);
			fsmgr_fptr = fopen(fsmgr_path, "w+");
			if (!fsmgr_fptr) {
				errcode = errno;
				return -errcode;
			}
			setbuf(fsmgr_fptr, NULL);
			ret = fetch_object_busywait_conn(fsmgr_fptr,
					RESTORE_FETCH_OBJ, objname, NULL);
			fclose(fsmgr_fptr);
			if (ret < 0) {
				unlink(fsmgr_path);
				return ret;
			}
		} else {
			return -errcode;
		}
	}
	fsmgr_fptr = fopen(fsmgr_path, "r+");
	if (!fsmgr_fptr) {
		errcode = errno;
		return -errcode;
	}
	flock(fileno(fsmgr_fptr), LOCK_EX);
	PREAD(fileno(fsmgr_fptr), &dirmeta, sizeof(DIR_META_TYPE), 16);
	ret = collect_dirmeta_children(&dirmeta, fsmgr_fptr, &dir_nodes,
			&num_dir, &nondir_nodes, &num_nondir, NULL, FALSE);
	if (ret < 0)
		return ret;
	flock(fileno(fsmgr_fptr), LOCK_UN);
	fclose(fsmgr_fptr);

	*num_inodes = num_dir + num_nondir; /* Collect all root inodes */
	if (*num_inodes > 0) {
		*roots = (ino_t *)malloc(sizeof(ino_t) * (*num_inodes));
		if (num_dir > 0)
			memcpy(*roots, dir_nodes, sizeof(ino_t) * num_dir);
		if (num_nondir > 0)
			memcpy(*roots + num_dir, nondir_nodes,
					sizeof(ino_t) * num_nondir);
	} else {
		*roots = NULL;
		write_log(4, "There are no root inodes\n");
		return -ENOENT;
	}

	free(dir_nodes);
	free(nondir_nodes);
	write_log(4, "Number of roots is %"PRId64, *num_inodes);

	return 0;
errcode_handle:
	flock(fileno(fsmgr_fptr), LOCK_UN);
	fclose(fsmgr_fptr);
	return errcode;
}

/**
 * Helper of init_rebuild_sb(). Initialize the queue file and push all
 * root inodes into the queue file.
 */
int32_t _init_rebuild_queue_file(ino_t *roots, int64_t num_roots)
{
	char queue_filepath[200];
	int32_t fh;
	int32_t errcode;

	sprintf(queue_filepath, "%s/rebuild_sb_queue", METAPATH);
	fh = open(queue_filepath, O_CREAT | O_RDWR, 0600);
	if (fh < 0) {
		errcode = errno;
		return -errcode;
	}
	PWRITE(fh, roots, sizeof(ino_t) * num_roots, 0);
	rebuild_sb_jobs->queue_fh = fh;
	rebuild_sb_jobs->remaining_jobs = num_roots;
	rebuild_sb_jobs->job_count = 0;

	rebuild_sb_jobs->cache_jobs.num_cached_inode = 0;
	rebuild_sb_jobs->cache_jobs.cache_idx = 0;
	return 0;

errcode_handle:
	return errcode;
}

/**
 * Helper of init_rebuild_sb(). Read from FS backend statistics file "FSstat<x>"
 * and find the maximum inode number. Finally create superblock and initialize
 * the superblock head.
 */
int32_t _init_sb_head(ino_t *roots, int64_t num_roots)
{
	ino_t root_inode, max_inode, tmp_ino;
	int64_t idx;
	int32_t ret;
	char fstatpath[400], fs_sync_path[400];
	char sb_path[300];
	char restore_todelete_list[METAPATHLEN];
	FILE *fptr, *sb_fptr;
	FS_CLOUD_STAT_T fs_cloud_stat;
	SUPER_BLOCK_HEAD head;
	struct stat tmpstat;
	int32_t errcode;

	sprintf(fs_sync_path, "%s/FS_sync", METAPATH);
	if (access(fs_sync_path, F_OK) < 0)
		MKDIR(fs_sync_path, 0700);

	max_inode = 1;
	/* Find maximum inode number */
	for (idx = 0 ; idx < num_roots ; idx++) {
		root_inode = roots[idx];
		write_log(10, "Debug: root %"PRIu64"\n", (uint64_t)root_inode);

		snprintf(fstatpath, METAPATHLEN - 1,
				"%s/FS_sync/FSstat%" PRIu64 "",
				METAPATH, (uint64_t)root_inode);
		/* FSstat should be downloaded in stage 1 already */
		if (access(fstatpath, F_OK) < 0) {
			errcode = errno;
			return -errcode;
		}
		/* Get max inode number from FS cloud stat */
		fptr = fopen(fstatpath, "r");
		if (!fptr) {
			errcode = errno;
			return -errcode;
		}
		FSEEK(fptr, 0, SEEK_SET);
		FREAD(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
		fclose(fptr);
		if (max_inode < (ino_t) fs_cloud_stat.max_inode)
			max_inode = (ino_t) fs_cloud_stat.max_inode;

		write_log(10, "Max inode is now %" PRIu64
		          ", per vol max is %" PRIu64 "\n",
		          (uint64_t) max_inode,
		          (uint64_t) fs_cloud_stat.max_inode);

		/* Check if FS local stat exist and init it. */
		fetch_stat_path(fstatpath, root_inode);
		if (access(fstatpath, F_OK) < 0) {
			FS_STAT_T fs_stat;

			fptr = fopen(fstatpath, "w+");
			if (!fptr) {
				errcode = errno;
				write_log(0, "Error: IO error in %s. Code %d",
					__func__, errcode);
				return -errcode;
			}
			fs_stat.system_size = fs_cloud_stat.backend_system_size;
			fs_stat.meta_size = fs_cloud_stat.backend_meta_size;
			fs_stat.num_inodes = fs_cloud_stat.backend_num_inodes;
			/* Need to check how many
			inodes are deleted in stage 1 already */
			snprintf(restore_todelete_list, METAPATHLEN,
				"%s/todelete_list_%" PRIu64,
			         METAPATH, (uint64_t) root_inode);
			if (access(restore_todelete_list, F_OK) == 0) {
				ret = stat(restore_todelete_list,
				           &tmpstat);
				if (ret == 0)
					fs_stat.num_inodes -=
						tmpstat.st_size /
						(sizeof(ino_t));
			}
			FWRITE(&fs_stat, sizeof(FS_STAT_T), 1, fptr);
			fclose(fptr);
		}
	}
	ret = read_system_max_inode(&tmp_ino);
	if (ret < 0)
		return ret;
	max_inode = (max_inode > tmp_ino ? max_inode : tmp_ino);

	write_log(0, "Now max inode number is %"PRIu64, (uint64_t)max_inode);
	/* init head */
	sprintf(sb_path, "%s/superblock", METAPATH);
	sb_fptr = fopen(sb_path, "w+");
	if (!sb_fptr)
		return -errno;
	memset(&head, 0, sizeof(SUPER_BLOCK_HEAD));
	head.num_total_inodes = max_inode;
	FSEEK(sb_fptr, 0, SEEK_SET);
	FWRITE(&head, sizeof(SUPER_BLOCK_HEAD), 1, sb_fptr);
	FTRUNCATE(fileno(sb_fptr), sizeof(SUPER_BLOCK_HEAD) +
			max_inode * sizeof(SUPER_BLOCK_ENTRY));
	fclose(sb_fptr);

	/* Reset and update superblock size */
	sem_wait(&(hcfs_system->access_sem));
	hcfs_system->systemdata.super_block_size = 0;
	sem_post(&(hcfs_system->access_sem));
	update_sb_size();

	return 0;

errcode_handle:
	return errcode;
}

/**
 * Init sb header, queuing file, and allocate some resource. If rebuild_action
 * is START_REBUILD_SB, then get root inodes and start to rebuild superblock.
 * Otherwise read queue file and continue to rebuild. Finally set flag
 * system_restoring as true.
 *
 * @param rebuild_action A flag used to distinguish between rebuild at the
 *                       begining and keep rebuilding using existed queue file.
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t init_rebuild_sb(char rebuild_action)
{
	char queue_filepath[200];
	int32_t ret, errcode;
	ino_t *roots = NULL;
	int64_t num_roots = 0;
	int64_t ret_pos;

	write_log(10, "Initing sb rebuilding\n");
	/* Allocate memory for jobs */
	rebuild_sb_jobs = (REBUILD_SB_JOBS *)
			calloc(sizeof(REBUILD_SB_JOBS), 1);
	if (!rebuild_sb_jobs) {
		write_log(0, "Error: Fail to allocate memory in %s\n",
				__func__);
		return -ENOMEM;
	}
	memset(rebuild_sb_jobs, 0, sizeof(REBUILD_SB_JOBS));
	pthread_mutex_init(&(rebuild_sb_jobs->job_mutex), NULL);
	pthread_cond_init(&(rebuild_sb_jobs->job_cond), NULL);
	sem_init(&(rebuild_sb_jobs->queue_file_sem), 0, 1);

	/* Allocate memory for mgr */
	rebuild_sb_tpool = (SB_THREAD_POOL *)
			calloc(sizeof(SB_THREAD_POOL), 1);
	if (!rebuild_sb_tpool) {
		write_log(0, "Error: Fail to allocate memory in %s\n",
				__func__);
		free(rebuild_sb_jobs);
		return -ENOMEM;
	}
	memset(rebuild_sb_tpool, 0, sizeof(SB_THREAD_POOL));
	rebuild_sb_tpool->tmaster = -1;
	sem_init(&(rebuild_sb_tpool->tpool_access_sem), 0, 1);

	sprintf(queue_filepath, "%s/rebuild_sb_queue", METAPATH);

	ret = _get_root_inodes(&(ROOT_INFO.roots), &(ROOT_INFO.num_roots));
	if (ret < 0)
		return ret;

	/* Rebuild superblock */
	if (rebuild_action == START_REBUILD_SB) {
		/* Get roots */
		ret = _get_root_inodes(&roots, &num_roots);
		if (ret < 0)
			return ret;
		/* Init queue file */
		ret = _init_rebuild_queue_file(roots, num_roots);
		if (ret < 0) {
			free(roots);
			return ret;
		}
		/* Init sb header */
		ret = _init_sb_head(roots, num_roots);
		if (ret < 0) {
			free(roots);
			return ret;
		}
		free(roots);

	} else if (rebuild_action == KEEP_REBUILD_SB){

		/* Check queue file and open it */
		if (access(queue_filepath, F_OK) == 0) {
			rebuild_sb_jobs->queue_fh =
				open(queue_filepath, O_RDWR);
			if (rebuild_sb_jobs->queue_fh <= 0) {
				errcode = errno;
				return -errcode;
			}
			ret_pos = lseek(rebuild_sb_jobs->queue_fh, 0, SEEK_END);
			rebuild_sb_jobs->remaining_jobs =
				ret_pos / sizeof(ino_t);
		} else {
			errcode = errno;
			if (errcode != ENOENT)
				return -errcode;
			ret = _get_root_inodes(&roots, &num_roots);
			if (ret < 0)
				return ret;
			ret = _init_rebuild_queue_file(roots, num_roots);
			if (ret < 0) {
				free(roots);
				return ret;
			}
			free(roots);
		}
	} else {
		write_log(0, "Error: Invalid type\n");
		return -EINVAL;
	}

	ret = super_block_init();
	if (ret < 0)
		return ret;

	/* load the rectified system meta */
	ret = init_rectified_system_meta(RESTORING_STAGE2);
	if (ret < 0)
		write_log(0, "Error: Fail to open rectified sys meta\n");

	/* FEATURE TODO: Add more error handling here as this is
	not the normal case for entering super block rebuild */
	/* Check system_restoring flag */
	sem_wait(&(hcfs_system->access_sem));
	if (hcfs_system->system_restoring != RESTORING_STAGE2)
		hcfs_system->system_restoring = RESTORING_STAGE2;
	sem_post(&(hcfs_system->access_sem));

	return 0;
}

/**
 * Destroy and reclaim resource of rebuilding superblock. This function
 * is called when system shutdown or rebuilding process completed. If system
 * is going to shutdown, do not remove the queue file so that it can keep
 * rebuilding superblock next time.
 *
 * @param destroy_queue_file Flag indicating if queue file can be removed.
 *
 * @return none.
 */
void destroy_rebuild_sb(BOOL destroy_queue_file)
{
	char queue_filepath[400];
	int32_t idx;

	for (idx = 0; idx < NUM_THREADS_IN_POOL; idx++)
		pthread_join(rebuild_sb_tpool->thread[idx].tid, NULL);

	close(rebuild_sb_jobs->queue_fh);

	if (destroy_queue_file == TRUE) {
		sprintf(queue_filepath, "%s/rebuild_sb_queue", METAPATH);
		if (!access(queue_filepath, F_OK))
			unlink(queue_filepath);
	}

	/* Free resource */
	pthread_mutex_destroy(&(rebuild_sb_jobs->job_mutex));
	pthread_cond_destroy(&(rebuild_sb_jobs->job_cond));
	sem_destroy(&(rebuild_sb_jobs->queue_file_sem));
	sem_destroy(&(rebuild_sb_tpool->tpool_access_sem));
	free(rebuild_sb_jobs);
	free(rebuild_sb_tpool);
	rebuild_sb_jobs = NULL;
	rebuild_sb_tpool = NULL;
	return;
}

/**
 * When backend comes from offline to online, backend monitor need to wake up
 * these sleeping worker threads if system is restoring mode.
 *
 * @return none.
 */
void wake_sb_rebuilder(void)
{
	/* Lock hcfs_system to avoid race condition */
	sem_wait(&hcfs_system->access_sem);
	if (hcfs_system->system_restoring != RESTORING_STAGE2) {
		sem_post(&hcfs_system->access_sem);
		return;
	}

	pthread_mutex_lock(&(rebuild_sb_jobs->job_mutex));
	if (rebuild_sb_jobs->job_finish == FALSE) {
		if (rebuild_sb_tpool->num_idle > 0) {
			pthread_cond_broadcast(
				&(rebuild_sb_jobs->job_cond));
		}
	}
	pthread_mutex_unlock(&(rebuild_sb_jobs->job_mutex));
	sem_post(&(hcfs_system->access_sem));
	return;
}

/**
 * Pull an inode job. Job mutex lock should be locked before calling
 * this function. If inode job in cache queue is empty, then read many
 * inode from queue file and cache them in the cache queue.
 *
 * @param inode_job Job handler to store inode job and its position
 *                  in queue file.
 *
 * @return 0 on success, -ENOENT in case of no inode job.
 */
int32_t pull_inode_job(INODE_JOB_HANDLE *inode_job)
{
	CACHED_JOBS *cache_job;
	ssize_t ret_ssize;

	if (rebuild_sb_jobs->remaining_jobs <= 0)
		return -ENOENT;

	inode_job->inode = 0;
	cache_job = &(rebuild_sb_jobs->cache_jobs);

	while (rebuild_sb_jobs->remaining_jobs > 0) {
		if (cache_job->cache_idx >= cache_job->num_cached_inode) {
			/* Fetch many inodes */
			LOCK_QUEUE_FILE();
			ret_ssize = pread(rebuild_sb_jobs->queue_fh,
				cache_job->cached_inodes,
				sizeof(ino_t) * NUM_CACHED_INODES,
				sizeof(ino_t) * rebuild_sb_jobs->job_count);
			UNLOCK_QUEUE_FILE();
			/* Set num of cached inodes */
			if (ret_ssize > 0) {
				cache_job->num_cached_inode =
						ret_ssize / sizeof(ino_t);
				cache_job->cache_idx = 0;
				if (cache_job->num_cached_inode == 0) {
					write_log(0,"Error: Queue file"
						" is corrupted?\n");
					rebuild_sb_jobs->remaining_jobs = 0;
					return -ENOENT;
				}
			} else {
				write_log(0, "Error: Queue file "
					"is corrupted?\n");
				rebuild_sb_jobs->remaining_jobs = 0;
				cache_job->num_cached_inode = 0;
				cache_job->cache_idx = 0;
				return -ENOENT;
			}
		}
		inode_job->inode =
			cache_job->cached_inodes[cache_job->cache_idx];
		inode_job->queue_file_pos = rebuild_sb_jobs->job_count *
			sizeof(ino_t);
		cache_job->cache_idx++;
		rebuild_sb_jobs->job_count++;
		rebuild_sb_jobs->remaining_jobs--;
		if (inode_job->inode > 0)
			break;
	}

	write_log(10, "Debug: Number of remaining inode is %"PRId64,
			rebuild_sb_jobs->remaining_jobs);
	if (inode_job->inode == 0)
		return -ENOENT;

	return 0;
}

/**
 * Push a bunch of inodes into queue file.
 *
 * @param inode_jobs Inode array to be pushed into queue file.
 * @param num_inodes Number of inodes in inode_jobs.
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t push_inode_job(ino_t *inode_jobs, int64_t num_inodes)
{
	int64_t total_jobs;

	if (inode_jobs == NULL || num_inodes <= 0)
		return 0;

	pthread_mutex_lock(&(rebuild_sb_jobs->job_mutex));
	total_jobs = rebuild_sb_jobs->remaining_jobs +
			rebuild_sb_jobs->job_count;
	rebuild_sb_jobs->remaining_jobs += num_inodes;

	LOCK_QUEUE_FILE();
	PWRITE(rebuild_sb_jobs->queue_fh, inode_jobs,
			sizeof(ino_t) * num_inodes,
			sizeof(ino_t) * total_jobs);
	UNLOCK_QUEUE_FILE();
	pthread_mutex_unlock(&(rebuild_sb_jobs->job_mutex));
	return 0;

errcode_handle:
	UNLOCK_QUEUE_FILE();
	pthread_mutex_unlock(&(rebuild_sb_jobs->job_mutex));
	return errcode;
}

/**
 * Erase an inode number in queue file.
 *
 * @param inode_job Job handle with inode number and pos in queue file.
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t erase_inode_job(INODE_JOB_HANDLE *inode_job)
{
	int64_t empty_inode;

	empty_inode = 0;
	LOCK_QUEUE_FILE();
	PWRITE(rebuild_sb_jobs->queue_fh, &empty_inode, sizeof(ino_t),
		inode_job->queue_file_pos);
	UNLOCK_QUEUE_FILE();
	return 0;

errcode_handle:
	UNLOCK_QUEUE_FILE();
	return errcode;
}


static void _change_worker_status(int32_t t_idx, char new_status)
{
	char old_status;

	old_status = rebuild_sb_tpool->thread[t_idx].status;

	sem_wait(&(rebuild_sb_tpool->tpool_access_sem));
	rebuild_sb_tpool->thread[t_idx].status = new_status;
	if (old_status == WORKING && new_status == IDLE)
		rebuild_sb_tpool->num_idle++;
	if (old_status == IDLE && new_status == WORKING)
		rebuild_sb_tpool->num_idle--;
	sem_post(&(rebuild_sb_tpool->tpool_access_sem));

}

static int32_t _worker_get_job(int32_t tidx, INODE_JOB_HANDLE *inode_job)
{
	int32_t ret;

	pthread_mutex_lock(&(rebuild_sb_jobs->job_mutex));
	while (1) {
		memset(inode_job, 0, sizeof(INODE_JOB_HANDLE));
		/* Check job and system status */
		if (hcfs_system->system_going_down) {
			if (rebuild_sb_tpool->num_idle > 0)
				pthread_cond_broadcast(
						&(rebuild_sb_jobs->job_cond));

			ret = -ESHUTDOWN;
			break;
		}

		if (rebuild_sb_jobs->job_finish == TRUE) {
			ret = -ENOENT;
			break;
		}

		/* Check whether all jobs completed */
		if (rebuild_sb_tpool->num_idle == (NUM_THREADS_IN_POOL - 1) &&
				rebuild_sb_jobs->remaining_jobs <= 0) {
			write_log(10, "Debug: Master is worker %d\n", tidx);
			rebuild_sb_jobs->job_finish = TRUE;
			/* Be master */
			sem_wait(&(rebuild_sb_tpool->tpool_access_sem));
			if (rebuild_sb_tpool->tmaster == -1) {
				write_log(10, "Debug: Master is worker %d\n",
					tidx);
				rebuild_sb_tpool->tmaster = tidx;
			}
			sem_post(&(rebuild_sb_tpool->tpool_access_sem));
			/* Wake them up */
			pthread_cond_broadcast(
					&(rebuild_sb_jobs->job_cond));
			ret = -ENOENT; /* All jobs completed */
			break;
		}

		/* Sleep if backend is offline or there are no jobs */
		if (hcfs_system->backend_is_online == FALSE ||
				rebuild_sb_jobs->remaining_jobs <= 0) {
			_change_worker_status(tidx, IDLE);
			/* Wait for job */
			pthread_cond_wait(&(rebuild_sb_jobs->job_cond),
					&(rebuild_sb_jobs->job_mutex));
			_change_worker_status(tidx, WORKING);
			continue; /* Check conn and system */
		} else {
			ret = pull_inode_job(inode_job);
			if (ret < 0) {
				if (ret == -ENOENT)
					continue;
				else
					write_log(0, "Error: Fail to"
						" pull job. Code %d\n", -ret);
			}
			break;
		}

	}
	pthread_mutex_unlock(&(rebuild_sb_jobs->job_mutex));

	return ret;
}

/**
 * Superblock rebuilder. Each thread get an inode number and try to restore
 * meta file and superblock entry. Once all the jobs are completed. The last
 * working thread will be the master and wake all the other threads up. The
 * master thread then reclaims unused inode numbers in superblock and finally
 * wake up fuse thread to set flag "system_restoring" as FALSE. It means
 * restoration is finished.
 *
 * @param t_idx Thread index of itself.
 *
 * @return none.
 */
void rebuild_sb_worker(void *t_idx)
{
	int32_t ret;
	int32_t tidx;
	BOOL leave;
	INODE_JOB_HANDLE inode_job;
	HCFS_STAT this_stat;

	write_log(4, "Now begin to rebuild sb\n");
	write_log(4, "Number of remaining jobs %lld\n",
			rebuild_sb_jobs->remaining_jobs);

	tidx = *(int32_t *)t_idx;

	leave = FALSE;
	while (1) {
		/* Get a job */
		ret = _worker_get_job(tidx, &inode_job);
		if (ret < 0) {
			if (ret == -ENOENT) {
				if (rebuild_sb_jobs->job_finish)
					break;
				else
					continue;
			} else if (ret == -ESHUTDOWN) {
				write_log(4, "System shutdown\n");
				leave = TRUE;
				break;
			}
		}
		write_log(10, "Debug: Woker %d begins to restore meta %"PRIu64,
				tidx, (uint64_t)(inode_job.inode));

		/* Restore meta file and rebuild this entry */
		ret = restore_meta_super_block_entry(inode_job.inode,
				&this_stat);
		if (ret < 0) {
			if (ret == -ENOENT) {
				write_log(0, "Error: meta %"PRIu64" not found."
					"  Cannot restore it",
					(uint64_t)inode_job.inode);
				erase_inode_job(&inode_job);
			} else {
				push_inode_job(&(inode_job.inode), 1);
				erase_inode_job(&inode_job);
			}
			continue;
		}

		/* Push new job */
		if (S_ISDIR(this_stat.mode)) {
			ino_t *dir_list, *nondir_list;
			int64_t num_dir, num_nondir;
			char *nondir_type_list;
			int32_t idx;

			dir_list = NULL;
			nondir_list = NULL;
			nondir_type_list = NULL;
			num_dir = 0;
			num_nondir = 0;

			ret = collect_dir_children(inode_job.inode,
				&dir_list, &num_dir, &nondir_list, &num_nondir,
				&nondir_type_list, FALSE);
			if (ret < 0) {
				push_inode_job(&(inode_job.inode), 1);
				erase_inode_job(&inode_job);
				free(dir_list);
				free(nondir_list);
				free(nondir_type_list);
				continue;
			}
			if (num_dir > 0) {
				/* Rebuild parent stat */
				for (idx = 0; idx < num_dir; idx++) {
					ret = rebuild_parent_stat(
						dir_list[idx],
						inode_job.inode, D_ISDIR);
					if (ret < 0)
						write_log(0, "Error: Fail"
							" to rebuild parent"
							" stat. Code %d", -ret);
				}
				ret = push_inode_job(dir_list, num_dir);
				if (ret < 0) {
					push_inode_job(&(inode_job.inode), 1);
					erase_inode_job(&inode_job);
					free(dir_list);
					free(nondir_list);
					free(nondir_type_list);
					continue;
				}
				free(dir_list);
			}
			if (num_nondir > 0) {
				/* Rebuild parent stat */
				for (idx = 0; idx < num_nondir; idx++) {
					ret = rebuild_parent_stat(
						nondir_list[idx],
						inode_job.inode,
						nondir_type_list[idx]);
					if (ret < 0)
						write_log(0, "Error: Fail"
							" to rebuild parent"
							" stat. Code %d", -ret);
				}
				ret = push_inode_job(nondir_list, num_nondir);
				if (ret < 0) {
					push_inode_job(&(inode_job.inode), 1);
					erase_inode_job(&inode_job);
					free(nondir_list);
					free(nondir_type_list);
					continue;
				}
				free(nondir_list);
				free(nondir_type_list);
			}

			/* Job completed */
			erase_inode_job(&inode_job);
			write_log(10, "Debug: Finish restore meta%"PRIu64,
					(uint64_t)inode_job.inode);

			pthread_mutex_lock(&(rebuild_sb_jobs->job_mutex));
			if (rebuild_sb_tpool->num_idle > 0 &&
				num_dir + num_nondir > 0)
				pthread_cond_broadcast(
						&(rebuild_sb_jobs->job_cond));
			pthread_mutex_unlock(&(rebuild_sb_jobs->job_mutex));

		} else {
			/* Job completed */
			erase_inode_job(&inode_job);
			write_log(10, "Debug: Finish restore meta%"PRIu64,
					(uint64_t)inode_job.inode);
		}
	}

	if (leave == TRUE)
		return;

	/* For thread not master, just come off work */
	sem_wait(&(rebuild_sb_tpool->tpool_access_sem));
	if (rebuild_sb_tpool->tmaster != tidx) {
		rebuild_sb_tpool->thread[tidx].active = FALSE;
		rebuild_sb_tpool->num_active--;
		sem_post(&(rebuild_sb_tpool->tpool_access_sem));
		write_log(10, "Worker %d comes off work. Happy!\n", tidx);
		return;
	}
	sem_post(&(rebuild_sb_tpool->tpool_access_sem));

	write_log(10, "Debug: Poor master is worker %d\n", tidx);

	/* Reclaim all unused inode */
	ret = super_block_reclaim_fullscan();
	if (ret < 0) {
		write_log(0, "Error: Fail to reclaim inodes. Code %d\n", -ret);
	}

	/* Rebuild completed. Set flag now_rebuild = FALSE */
	super_block_exclusive_locking();
	//sys_super_block->head.now_rebuild = FALSE;
	write_super_block_head();
	super_block_exclusive_release();
	write_log(10, "Debug: Rebuilding superblock completed.");

	ret = rectify_space_usage();
	if (ret < 0)
		write_log(0, "Error: Error in rectifying. Code %d\n", -ret);
	sem_wait(&(rebuild_sb_tpool->tpool_access_sem));
	rebuild_sb_tpool->thread[tidx].active = FALSE;
	rebuild_sb_tpool->num_active--;
	sem_post(&(rebuild_sb_tpool->tpool_access_sem));

	sem_post(&(hcfs_system->fuse_sem));
	return;
}

/**
 * Create many threads to rebuild super block.
 *
 * @return 0 on success.
 */
int32_t create_sb_rebuilder()
{
	int32_t idx;

	if (CURRENT_BACKEND == NONE) {
		write_log(5, "Cannot restore without network conn\n");
		return -EPERM;
	}
	sem_wait(&(rebuild_sb_tpool->tpool_access_sem));
	for (idx = 0; idx < NUM_THREADS_IN_POOL; idx++) {
		pthread_attr_init(
			&(rebuild_sb_tpool->thread[idx].t_attr));
		pthread_attr_setdetachstate(
			&(rebuild_sb_tpool->thread[idx].t_attr),
			PTHREAD_CREATE_DETACHED);
		rebuild_sb_tpool->tidx[idx] = idx;
		rebuild_sb_tpool->thread[idx].active = TRUE;
		rebuild_sb_tpool->thread[idx].status = WORKING;
		rebuild_sb_tpool->num_active += 1;
		pthread_create(&(rebuild_sb_tpool->thread[idx].tid),
			NULL, (void *)rebuild_sb_worker,
			(void *)&(rebuild_sb_tpool->tidx[idx]));
	}
	sem_post(&(rebuild_sb_tpool->tpool_access_sem));
	return 0;
}

/**
 * Rebuild a super block entry. Do nothing when this entry had been
 * rebuilded before.
 *
 * @param this_inode Entry of the inode in superblock to be rebuild.
 * @param this_stat Stat data to be filled into superblock entry.
 * @param pin_status Pin status of the file.
 *
 * @return 0 on success. Otherwise negative error code.
 */
int32_t rebuild_super_block_entry(ino_t this_inode,
	HCFS_STAT *this_stat, char pin_status)
{
	int32_t ret;
	SUPER_BLOCK_ENTRY sb_entry, tmp_sb_entry;

	/* Check whether this entry had been rebuilded */
	ret = super_block_read(this_inode, &sb_entry);
	if (ret < 0)
		return ret;
	if (sb_entry.this_index > 0)
		return 0;

	/* Rebuild entry */
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	memcpy(&(sb_entry.inode_stat), this_stat, sizeof(HCFS_STAT));
	sb_entry.this_index = this_inode;
	sb_entry.generation = 1;
	sb_entry.status = NO_LL;
	sb_entry.pin_status = ST_UNPIN;
	sb_entry.lastsync_time = get_current_sectime();

	super_block_exclusive_locking();
	/* Check again */
	ret = read_super_block_entry(this_inode, &tmp_sb_entry);
	if (ret < 0) {
		super_block_exclusive_release();
		return ret;
	}
	if (tmp_sb_entry.this_index > 0) {
		super_block_exclusive_release();
		return 0;
	}

	/* Begin to write entry */
	if (pin_status == PIN) {
		if (S_ISREG(this_stat->mode)) {
			/* Enqueue and set as ST_PINNING */
			ret = pin_ll_enqueue(this_inode, &sb_entry);
			if (ret < 0) {
				super_block_exclusive_release();
				return ret;
			}
			sb_entry.pin_status = ST_PINNING;
		} else {
			sb_entry.pin_status = ST_PIN;
		}
	}
	ret = write_super_block_entry(this_inode, &sb_entry);
	if (ret < 0) {
		super_block_exclusive_release();
		return ret;
	}
	super_block_exclusive_release();
	write_log(10, "Debug: Rebuilding sb entry %"PRIu64" has completed.\n",
			(uint64_t)this_inode);
	return 0;
}

/**
 * Find out all entries with inode number "this_inode" in parent folder
 * pointed by parent_ptr.
 */
int32_t _dir_find_all_inodes(ino_t this_inode, int32_t *num_entries,
	DIR_ENTRY **entry_list, META_CACHE_ENTRY_STRUCT *parent_ptr,
	BOOL *is_external)
{
	DIR_META_TYPE tempmeta;
	DIR_ENTRY_PAGE temp_page;
	int32_t ret, errcode;
	int32_t count;
	int32_t now_size, max_size;
	DIR_ENTRY *tmpptr;

	*is_external = FALSE;
	max_size = 5;
	now_size = 0;
	*entry_list = (DIR_ENTRY *) malloc(sizeof(DIR_ENTRY) * max_size);
	if (*entry_list == NULL)
		return -ENOMEM;

	ret = meta_cache_lookup_dir_data(parent_ptr->inode_num, NULL, &tempmeta,
						NULL, parent_ptr);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	/* Find out all entries */
	memset(&temp_page, 0, sizeof(DIR_ENTRY_PAGE));
	temp_page.this_page_pos = tempmeta.tree_walk_list_head;
	while (temp_page.this_page_pos != 0) {
		ret = meta_cache_lookup_dir_data(parent_ptr->inode_num, NULL,
						NULL, &temp_page, parent_ptr);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}

		for (count = 0; count < temp_page.num_entries; count++) {
			if (temp_page.dir_entries[count].d_ino != this_inode)
				continue;
			memcpy(&((*entry_list)[now_size]),
					&(temp_page.dir_entries[count]),
					sizeof(DIR_ENTRY));
			now_size++;
			if (now_size >= max_size) {
				max_size += 10;
				tmpptr = (DIR_ENTRY *) realloc(*entry_list,
						max_size * sizeof(DIR_ENTRY));
				if (tmpptr == NULL) {
					errcode = -ENOMEM;
					goto errcode_handle;
				}
				*entry_list = tmpptr;
			}
		}
		temp_page.this_page_pos = temp_page.tree_walk_next;
	}

	*num_entries = now_size;
	if (now_size == 0) {
		errcode = -ENOENT;
		goto errcode_handle;
	}

	return 0;

errcode_handle:
	FREE(*entry_list);
	*num_entries = 0;
	return errcode;
}

/**
 * Given an inode number "this_inode", remove all entries in parent folders
 * and delete parent lookup table.
 */
int32_t prune_this_entry(ino_t this_inode)
{
	ino_t *parentlist = NULL;
	ino_t parent_inode;
	int32_t parentnum;
	int32_t ret, idx;
	DIR_ENTRY *entry_list = NULL;
	int32_t count, num_entries;
	META_CACHE_ENTRY_STRUCT *body_ptr = NULL;
	BOOL is_external;
	mode_t this_mode;

	sem_wait(&pathlookup_data_lock);
	ret = fetch_all_parents(this_inode, &parentnum, &parentlist);
	sem_post(&pathlookup_data_lock);
	if (ret < 0)
		write_log(0, "Error: Fail to fetch parent in %s. Code %d",
			__func__, -ret);
	if (parentnum == 0) {
		/* No parent, it may be removed by others. Just do nothing. */
		write_log(6, "Info: inode %"PRIu64" has no parents",
				(uint64_t)this_inode);
		return 0;
	} else {
		for (idx = 0; idx < parentnum; idx++)
			write_log(6, "Info: inode %"PRIu64" has parent inode %"
				PRIu64, (uint64_t)this_inode,
				(uint64_t)parentlist[idx]);
	}

	for (idx = 0; idx < parentnum; idx++) {
		parent_inode = parentlist[idx];
		/* If this parent is found in list, then the parent meta
		 * had been downloaded. Hence we can lock its meta cache. */
		body_ptr = meta_cache_lock_entry(parent_inode);
		if (body_ptr == NULL) {
			write_log(0, "Error: Fail to lock parent inode %"
				PRIu64" in %s. Skip to prune.",
				(uint64_t)parent_inode, __func__);
			continue;
		}
		ret = _dir_find_all_inodes(this_inode, &num_entries,
				&entry_list, body_ptr, &is_external);
		if (ret < 0) {
			if (ret == -ENOENT) {
				write_log(4, "Warn: inode %"PRIu64" not exist"
					" in parent inode %"PRIu64, (uint64_t)
					this_inode, (uint64_t)parent_inode);
			} else {
				write_log(0, "Error: Fail to fetch inode in %s."
					" Code %d", __func__, -ret);
			}
			meta_cache_unlock_entry(body_ptr);
			continue;
		}
		/* Each entry ahs the same type and inode num */
		switch (entry_list[0].d_type) {
		case D_ISREG: this_mode = S_IFREG; break;
		case D_ISDIR: this_mode = S_IFDIR; break;
		case D_ISLNK: this_mode = S_IFLNK; break;
		case D_ISFIFO: this_mode = S_IFIFO; break;
		case D_ISSOCK: this_mode = S_IFSOCK; break;
		default:
			write_log(0, "Error: Type is %d",
				entry_list[0].d_type);
			FREE(entry_list);
			meta_cache_unlock_entry(body_ptr);
			continue;
		}
		/* Remove all entries */
		for (count = 0; count < num_entries; count++) {
			ret = dir_remove_entry(parent_inode, this_inode,
				entry_list[count].d_name, this_mode,
				body_ptr, is_external);
			if (ret < 0 && ret != ENOENT) /* Do not care ENOENT */
				write_log(0, "Error: Fail to remove entry %s."
					" Code %d", entry_list[count].d_name,
					-ret);
		}

		FREE(entry_list);
		meta_cache_unlock_entry(body_ptr);

		/* Delete parent lookup entry */
		sem_wait(&(pathlookup_data_lock));
		ret = lookup_delete_parent(this_inode, parent_inode);
		if (ret < 0 && ret != ENOENT)
			write_log(0, "Error: Fail to delete parent. Code %d",
					-ret);
		sem_post(&(pathlookup_data_lock));
	}

	return 0;
}

/**
 * Given an inode number, restore meta and then rebuild super block entry.
 * This function first check super block entry so that ensure whether this
 * meta and super block entry had been restored. If it inode numebr field is
 * empty, then try to restore meta file and rebuild super block entry.
 * "ret_stat" will store stat data of an inode meta.
 *
 * @param this_inode Inode numebr to be restored.
 * @param ret_stat Stat data to be returned to caller.
 *
 * @return 0 on success. Otherwise negative error code.
 */
int32_t restore_meta_super_block_entry(ino_t this_inode, HCFS_STAT *ret_stat)
{
	char metapath[300];
	int32_t ret;
	char pin_status;
	HCFS_STAT this_stat;
	FILE_META_TYPE this_meta;
	FILE *fptr;
	SUPER_BLOCK_ENTRY sb_entry;
	int32_t errcode;

	/* Check whether this entry had been rebuilded */
	ret = super_block_read(this_inode, &sb_entry);
	if (ret < 0)
		return ret;
	if (sb_entry.this_index > 0) {
		if (sb_entry.status == TO_BE_RECLAIMED ||
				sb_entry.status == RECLAIMED ||
				sb_entry.status == TO_BE_DELETED ||
				sb_entry.inode_stat.ino == 0) {
			write_log(4, "Warn: Try to restore inode %"
				PRIu64" with status %d", (uint64_t)this_inode,
				sb_entry.status);
			return -ENOENT;
		}

		if (ret_stat)
			memcpy(ret_stat, &(sb_entry.inode_stat),
					sizeof(HCFS_STAT));
		return 0;
	}
	if (sb_entry.status == TO_BE_RECLAIMED) {
		/* This file had been removed since object not found, but
		 * still need to try to prune entries for those parent relation
		 * established after last pruning */
		prune_this_entry(this_inode);
		return -ENOENT;
	}

	/* Restore meta file */
	ret = restore_meta_file(this_inode);
	if (ret < 0) {
		/* Exception handling when meta is not
		 * found on cloud */
		if (ret == -ENOENT) {
			write_log(4, "Warn: Begin to remove inode %"PRIu64
				" from parent", (uint64_t)this_inode);
			prune_this_entry(this_inode);
			super_block_exclusive_locking();
			read_super_block_entry(this_inode, &sb_entry);
			if (sb_entry.status != TO_BE_RECLAIMED) {
				/* Set status so that it will not be fetched
				 * from backend again */
				sb_entry.status = TO_BE_RECLAIMED;
				write_super_block_entry(this_inode, &sb_entry);
			}
			super_block_exclusive_release();
		}
		write_log(2, "Warn: Fail to restore meta%"PRIu64". Code %d",
			(uint64_t)this_inode, -ret);
		return ret;
	}

	fetch_meta_path(metapath, this_inode);
	fptr = fopen(metapath, "r");
	if (!fptr) {
		/* Something wrong? */
		errcode = errno;
		write_log(0, "Error: Fail to open meta in %s. Code %d\n",
				__func__, errcode);
		return -errcode;
	}
	flock(fileno(fptr), LOCK_EX);
	/* Get pin status */
	FSEEK(fptr, 0, SEEK_SET);
	FREAD(&this_stat, sizeof(HCFS_STAT), 1, fptr);
	if (S_ISREG(this_stat.mode)) {
		FSEEK(fptr, sizeof(HCFS_STAT), SEEK_SET);
		FREAD(&this_meta, sizeof(FILE_META_TYPE), 1, fptr);
		if (P_IS_PIN(this_meta.local_pin))
			pin_status = PIN;
		else
			pin_status = UNPIN;
	} else {
		pin_status = PIN;
	}
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);

	/* Rebuild sb entry */
	ret = rebuild_super_block_entry(this_inode,
		&this_stat, pin_status);
	if (ret < 0) {
		write_log(2, "Warn: Fail to rebuild meta%"PRIu64
			" superblock entry\n", (uint64_t)this_inode);
		return ret;
	}

	if (ret_stat)
		memcpy(ret_stat, &this_stat, sizeof(HCFS_STAT));
	return 0;

errcode_handle:
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	return errcode;
}
