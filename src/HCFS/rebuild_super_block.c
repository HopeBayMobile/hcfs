#include "rebuild_super_block.h"

#include <unistd.h>
#include <sys/file.h>
#include <string.h>

#include "logger.h"
#include "errno.h"
#include "params.h"
#include "utils.h"
#include "metaops.h"
#include "macro.h"
#include "hcfs_fromcloud.h"

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
	int64_t ret_ssize;
	int64_t num_dir, num_nondir;
	ino_t *dir_nodes, *nondir_nodes;

	sprintf(fsmgr_path, "%s/fsmgr", METAPATH);
	if (access(fsmgr_path, F_OK) < 0) {
		errcode = errno;
		if (errcode == ENOENT) {
			/* Get fsmgr from cloud */
			sprintf(objname, "FSmgr_backup");
			fsmgr_fptr = fopen(fsmgr_path, "w+");
			if (!fsmgr_fptr) {
				errcode = errno;
				return -errcode;
			}
			flock(fileno(fsmgr_fptr), LOCK_EX);
			ret = fetch_object_from_cloud(fsmgr_fptr, objname);
			flock(fileno(fsmgr_fptr), LOCK_UN);
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
			&num_dir, &nondir_nodes, &num_nondir);
	if (ret < 0)
		return ret;
	flock(fileno(fsmgr_fptr), LOCK_UN);
	fclose(fsmgr_fptr);

	*num_inodes = num_dir + num_nondir; /* Collect all root inodes */
	*roots = (ino_t *)malloc(sizeof(ino_t) * (*num_inodes));
	memcpy(*roots, dir_nodes, sizeof(ino_t) * num_dir);
	memcpy(*roots + num_dir, nondir_nodes, sizeof(ino_t) * num_nondir);
	
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
	int32_t errcode;
	int32_t fh;
	ssize_t ret_ssize;

	sprintf(queue_filepath, "%s/rebuild_sb_queue", METAPATH);
	fh = open(queue_filepath, O_CREAT | O_RDWR);
	if (fh < 0) {
		errcode = errno;
		return -errcode;
	}
	PWRITE(fh, roots, sizeof(ino_t) * num_roots, 0);
	rebuild_sb_jobs->queue_fh = fh;
	rebuild_sb_jobs->remaining_jobs = num_roots;
	rebuild_sb_jobs->job_count = 0;

	return 0;

errcode_handle:
	return errcode;
}

/**
 * Helper of init_rebuild_sb(). Download FS backend statistics file "FSstat<x>"
 * and find the maximum inode number. Finally create superblock and initialize
 * the superblock head.
 */
int32_t _init_sb_head(ino_t *roots, int64_t num_roots)
{
	ino_t root_inode, max_inode;
	int64_t idx;
	int32_t ret, errcode;
	size_t ret_size;
	char fstatpath[300], sb_path[300];
	char objname[300];
	FILE *fptr, *sb_fptr;
	FS_CLOUD_STAT_T fs_cloud_stat;
	SUPER_BLOCK_HEAD head;

	max_inode = 1;
	/* Find maximum inode number */
	for (idx = 0 ; idx < num_roots ; idx++) {
		root_inode = roots[idx];
		snprintf(fstatpath, METAPATHLEN - 1,
				"%s/FS_sync/FSstat%" PRIu64 "",
				METAPATH, (uint64_t)root_inode);
		if (access(fstatpath, F_OK) < 0) {
			errcode = errno;
			if (errcode == ENOENT) {
				/* Get FSstat from cloud */
				sprintf(objname, "FSstat%"PRIu64,
					(uint64_t)root_inode);
				fptr = fopen(fstatpath, "w+");
				if (!fptr) {
					errcode = errno;
					return -errcode;
				}
				flock(fileno(fptr), LOCK_EX);
				ret = fetch_object_from_cloud(fptr,
						objname);
				flock(fileno(fptr), LOCK_UN);
				fclose(fptr);
				if (ret < 0) {
					unlink(fstatpath);
					return ret;
				}
			} else {
				return -errcode;
			}
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
		if (max_inode < fs_cloud_stat.max_inode)
			max_inode = fs_cloud_stat.max_inode;

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
			FWRITE(&fs_stat, sizeof(FS_STAT_T), 1, fptr);
			fclose(fptr);
		}
	}

	write_log(0, "Now max inode number is %"PRIu64, (uint64_t)max_inode);
	/* init head */
	sprintf(sb_path, "%s/superblock", METAPATH);
	sb_fptr = fopen(sb_path, "w+");
	if (!sb_fptr)
		return -errno;
	memset(&head, 0, sizeof(SUPER_BLOCK_HEAD));
	head.num_total_inodes = max_inode;
	head.now_rebuild = TRUE;
	FSEEK(sb_fptr, 0, SEEK_SET);
	FWRITE(&head, sizeof(SUPER_BLOCK_HEAD), 1, sb_fptr);
	FTRUNCATE(fileno(sb_fptr), sizeof(SUPER_BLOCK_HEAD) +
			max_inode * sizeof(SUPER_BLOCK_ENTRY));
	fclose(sb_fptr);

	return 0;

errcode_handle:
	return errcode;
}

/**
 * Init rebuild superblock. Init sb header, queuing file.
 */
int32_t init_rebuild_sb(char rebuild_action)
{
	char queue_filepath[200];
	int32_t ret, errcode;
	ino_t *roots;
	int64_t num_roots;
	int64_t ret_pos;

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

	/* Rebuild superblock */
	if (rebuild_action == START_REBUILD_SB) {
		/* Get roots */
		ret = _get_root_inodes(&roots, &num_roots);
		if (ret < 0)
			return ret;
		/* Init queue file */
		ret = _init_rebuild_queue_file(roots, num_roots);
		if (ret < 0)
			return ret;
		/* Init sb header */
		ret = _init_sb_head(roots, num_roots);
		if (ret < 0)
			return ret;

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
			if (ret < 0)
				return ret;
		}
	} else {
		write_log(0, "Error: Invalid type\n");
		return -EINVAL;
	}

	ret = super_block_init();
	if (ret < 0)
		return ret;

	return 0;
}

/* Need mutex lock */
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
			flock(rebuild_sb_jobs->queue_fh, LOCK_EX);
			ret_ssize = pread(rebuild_sb_jobs->queue_fh,
				cache_job->cached_inodes,
				sizeof(ino_t) * NUM_CACHED_INODES,
				sizeof(ino_t) * rebuild_sb_jobs->job_count);
			flock(rebuild_sb_jobs->queue_fh, LOCK_UN);
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

/* Do not need pre-lock job queue */
int32_t push_inode_job(ino_t *inode_jobs, int64_t num_inodes)
{
	int64_t total_jobs;
	ssize_t ret_ssize;
	int32_t errcode;

	pthread_mutex_lock(&(rebuild_sb_jobs->job_mutex));
	total_jobs = rebuild_sb_jobs->remaining_jobs +
			rebuild_sb_jobs->job_count;
	rebuild_sb_jobs->remaining_jobs += num_inodes;

	flock(rebuild_sb_jobs->queue_fh, LOCK_EX);
	PWRITE(rebuild_sb_jobs->queue_fh, inode_jobs,
			sizeof(ino_t) * num_inodes,
			sizeof(ino_t) * total_jobs);
	flock(rebuild_sb_jobs->queue_fh, LOCK_UN);
	pthread_mutex_unlock(&(rebuild_sb_jobs->job_mutex));
	return 0;

errcode_handle:
	flock(rebuild_sb_jobs->queue_fh, LOCK_UN);
	pthread_mutex_unlock(&(rebuild_sb_jobs->job_mutex));
	return errcode;
}

int32_t erase_inode_job(INODE_JOB_HANDLE *inode_job)
{
	int64_t empty_inode;
	ssize_t ret_ssize;
	int32_t errcode;

	empty_inode = 0;
	flock(rebuild_sb_jobs->queue_fh, LOCK_EX);
	PWRITE(rebuild_sb_jobs->queue_fh, &empty_inode, 1,
		inode_job->queue_file_pos);
	flock(rebuild_sb_jobs->queue_fh, LOCK_UN);
	return 0;

errcode_handle:
	flock(rebuild_sb_jobs->queue_fh, LOCK_UN);
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

	ret = 0;
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

		if (rebuild_sb_jobs->remaining_jobs <= 0) {
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
					write_log(0, "Error:");
			}
			break;
		}

	}
	pthread_mutex_unlock(&(rebuild_sb_jobs->job_mutex));

	return ret;
}

void rebuild_sb_worker(void *t_idx)
{
	int32_t ret;
	int32_t tidx;
	BOOL leave;
	INODE_JOB_HANDLE inode_job;
	struct stat this_stat;

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
				write_log(4, "Rebuilding all inodes completed\n");
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
		write_log(10, "Debug: Begin to restore meta %"PRIu64,
			(uint64_t)(inode_job.inode));

		/* Restore meta file and rebuild this entry */
		ret = restore_meta_super_block_entry(inode_job.inode,
				&this_stat);
		if (ret < 0) {
			push_inode_job(&(inode_job.inode), 1);
			erase_inode_job(&inode_job);
			continue;
		}

		/* Push new job */
		if (S_ISDIR(this_stat.st_mode)) {
			ino_t *dir_list, *nondir_list;
			int64_t num_dir, num_nondir;
			ret = collect_dir_children(inode_job.inode,
				&dir_list, &num_dir, &nondir_list, &num_nondir);
			if (ret < 0) {
				push_inode_job(&(inode_job.inode), 1);
				erase_inode_job(&inode_job);
				continue;
			}
			if (num_dir > 0) {
				ret = push_inode_job(dir_list, num_dir);
				if (ret < 0) {
					push_inode_job(&(inode_job.inode), 1);
					erase_inode_job(&inode_job);
					continue;
				}
			}
			if (num_nondir > 0) {
				ret = push_inode_job(nondir_list, num_nondir);
				if (ret < 0) {
					push_inode_job(&(inode_job.inode), 1);
					erase_inode_job(&inode_job);
					continue;
				}
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
		write_log(10, "Worker %d comes off work :)\n", tidx);
		return;
	}
	sem_post(&(rebuild_sb_tpool->tpool_access_sem));

	/* Reclaim all unused inode */
	write_log(10, "Debug: Poor master is %d\n", tidx);

	return;
}

int32_t create_sb_rebuilder()
{
	int32_t idx;

	if (CURRENT_BACKEND == NONE) {
		write_log(5, "Cannot restore without network conn\n");
		return -EPERM;
	}
	//sem_wait(&(rebuild_sb_mgr_info->mgr_access_sem));
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
			&(rebuild_sb_tpool->thread[idx].t_attr),
			(void *)rebuild_sb_worker,
			(void *)&(rebuild_sb_tpool->tidx[idx]));
	}
	sem_post(&(rebuild_sb_tpool->tpool_access_sem));
	//sem_post(&(rebuild_sb_mgr_info->mgr_access_sem));
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
	struct stat *this_stat, char pin_status)
{
	int32_t ret;
	SUPER_BLOCK_ENTRY sb_entry;

	/* Check whether this entry had been rebuilded */
	ret = super_block_read(this_inode, &sb_entry);
	if (ret < 0)
		return ret;
	if (sb_entry.this_index > 0)
		return 0;

	/* Rebuild entry */
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	memcpy(&(sb_entry.inode_stat), this_stat, sizeof(struct stat));
	sb_entry.this_index = this_inode;
	sb_entry.generation = 1;
	sb_entry.status = NO_LL;
	sb_entry.pin_status = ST_UNPIN;

	super_block_exclusive_locking();
	if (pin_status == PIN) {
		if (S_ISREG(this_stat->st_mode)) {
			/* Enqueue and set as ST_PINNING */
			ret = pin_ll_enqueue(this_inode, &sb_entry);
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
int32_t restore_meta_super_block_entry(ino_t this_inode, struct stat *ret_stat)
{
	char metapath[300];
	int32_t errcode, ret;
	size_t ret_size;
	char pin_status;
	struct stat this_stat;
	FILE_META_TYPE this_meta;
	FILE *fptr;
	SUPER_BLOCK_ENTRY sb_entry;

	/* Check whether this entry had been rebuilded */
	ret = super_block_read(this_inode, &sb_entry);
/* FEATURE TODO: Check if truncate super block to max inode num in
restoring mode is early enough so that read will always be successful here */
	if (ret < 0)
		return ret;
	if (sb_entry.this_index > 0) {
		if (ret_stat)
			memcpy(ret_stat, &(sb_entry.inode_stat),
					sizeof(struct stat));
		return 0;
	}

	/* Restore meta file */
	ret = restore_meta_file(this_inode);
	if (ret < 0) {
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
	FSEEK(fptr, 0, SEEK_SET);
	FREAD(&this_stat, sizeof(struct stat), 1, fptr);
	if (S_ISREG(this_stat.st_mode)) {
		FSEEK(fptr, sizeof(struct stat), SEEK_SET);
		FREAD(&this_meta, sizeof(FILE_META_TYPE), 1, fptr);
		pin_status = this_meta.local_pin == TRUE ? PIN : UNPIN;
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
		memcpy(ret_stat, &this_stat, sizeof(struct stat));
	return 0;

errcode_handle:
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	return errcode;
}

