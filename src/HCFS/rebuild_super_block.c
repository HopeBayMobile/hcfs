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

int32_t _get_root_inodes(ino_t **roots, int64_t *num_inodes)
{
	char fsmgr_path[200];
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
			/* TODO:Get fsmgr from cloud */
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
	*num_inodes = num_dir;
	*roots = dir_nodes;

	return 0;
errcode_handle:
	flock(fileno(fsmgr_fptr), LOCK_UN);
	fclose(fsmgr_fptr);
	return errcode;
}

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

int32_t _init_sb_head(ino_t *roots, int64_t num_roots)
{
	ino_t root_inode, max_inode;
	int64_t idx;
	int32_t ret, errcode;
	size_t ret_size;
	char fstatpath[300], sb_path[300];
	FILE *fptr, *sb_fptr;
	FS_CLOUD_STAT_T fs_cloud_stat;
	SUPER_BLOCK_HEAD head;

	max_inode = 1;
	for (idx = 0 ; idx < num_roots ; idx++) {
		root_inode = roots[idx];
		snprintf(fstatpath, METAPATHLEN - 1,
				"%s/FS_sync/FSstat%" PRIu64 "",
				METAPATH, (uint64_t)root_inode);
		if (access(fstatpath, F_OK) < 0) {
			errcode = errno;
			if (errcode == ENOENT) {
				/* TODO:Get FSstat from cloud */
			} else {
				return -errcode;
			}
		}
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
	}

	/* init head */
	sprintf(sb_path, "%s/superblock", METAPATH);
	sb_fptr = fopen(sb_path, "w+");
	if (!sb_fptr)
		return -errno;
	memset(&head, 0, sizeof(SUPER_BLOCK_HEAD));
	head.num_total_inodes = max_inode;
	head.now_rebuild = TRUE;
	FSEEK(fptr, 0, SEEK_SET);
	FWRITE(&head, sizeof(SUPER_BLOCK_HEAD), 1, sb_fptr);
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

	/* Allocate memory for mgr */
	rebuild_sb_mgr_info = (REBUILD_SB_MGR_INFO *)
			calloc(sizeof(REBUILD_SB_MGR_INFO), 1);
	if (!rebuild_sb_mgr_info) {
		write_log(0, "Error: Fail to allocate memory in %s\n",
				__func__);
		free(rebuild_sb_jobs);
		return -ENOMEM;
	}

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
	return 0;
}

/* Need mutex lock */
int32_t pull_inode_job(ino_t *inode_job)
{
	CACHED_JOBS *cache_job;
	ssize_t ret_ssize;

	if (rebuild_sb_jobs->remaining_jobs <= 0)
		return -ENOENT;

	*inode_job = 0;
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
		*inode_job = cache_job->cached_inodes[cache_job->cache_idx];
		cache_job->cache_idx++;
		rebuild_sb_jobs->job_count++;
		rebuild_sb_jobs->remaining_jobs--;
		if (*inode_job > 0)
			break;
	}

	if (*inode_job == 0)
		return -ENOENT;

	return 0;
}

int32_t rebuild_sb_manager()
{
	/* Create queue file */
	/* Get root inodes if file is empty */
	/* write to queue file */
	/* Create worker threads */
	/* Wait and check finish */
	/* Join all threads */
	return 0;
}

int32_t create_sb_rebuilder()
{
	if (CURRENT_BACKEND == NONE) {
		write_log(5, "Cannot restore without network conn\n");
		return -EPERM;
	}
	return 0;
}
