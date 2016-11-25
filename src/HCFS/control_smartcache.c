/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: control_smartcache.c
* Abstract: The c source code file for controlling mounting of smart cache.
*
* Revision History
* 2016/10/20 Kewei created this file.
*
**************************************************************************/
#include "control_smartcache.h"

#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "macro.h"
#include "logger.h"
#include "fuseop.h"
#include "do_restoration.h"
#include "meta.h"
#include "meta_mem_cache.h"
#include "super_block.h"
#include "metaops.h"
#include "parent_lookup.h"
#include "mount_manager.h"
#include "FS_manager.h"
#include "meta_iterator.h"

/**
 * Unmount smart cache.
 *
 * @param mount_point Mount point of this smart cache.
 *
 * @return 0 on success, otherwise error code.
 */
int32_t unmount_smart_cache(char *mount_point)
{
	int32_t ret;

	ret = umount(mount_point);
	if (ret < 0)
		ret = -errno;

	return ret;
}

static void _change_stage1_cache_limit(int64_t restored_smartcache_size)
{
	CACHE_HARD_LIMIT += restored_smartcache_size;

	/* Change the max system size as well */
	hcfs_system->systemdata.system_quota = CACHE_HARD_LIMIT;
	system_config->max_cache_limit[P_UNPIN] = CACHE_HARD_LIMIT;
	system_config->max_pinned_limit[P_UNPIN] += restored_smartcache_size;

	system_config->max_cache_limit[P_PIN] = CACHE_HARD_LIMIT;
	system_config->max_pinned_limit[P_PIN] += restored_smartcache_size;

	system_config->max_cache_limit[P_HIGH_PRI_PIN] =
	    			CACHE_HARD_LIMIT + RESERVED_CACHE_SPACE;
	system_config->max_pinned_limit[P_HIGH_PRI_PIN] +=
				restored_smartcache_size;

	/* Update pin size */
}

/**
 * Use function "system" to execute "command".
 *
 * @return 0 on success, otherwise -EPERM on error.
 */
static int32_t _run_command(char *command)
{
	int32_t status, errcode;

	status = system(command);
	if (status < 0) {
		errcode = errno;
		write_log(0, "Error: Fail to set loop dev in %s. Code %d",
				__func__, errcode);
		return -errcode;
	}

	write_log(4, "Test: status code: %d. Command: %s", status, command);

	if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) != EXIT_SUCCESS) {
			write_log(0, "Return status in %s: %d",
				__func__, WEXITSTATUS(status));
			return -EPERM;
		}
	} else {
		write_log(0, "Return status in %s: %d", __func__, status);
		return -EAGAIN;
	}

	return 0;
}


/**
 * Inject restored smart cache data and meta to now active HCFS. The restored
 * smart cache will be placed under HCFS mount point /data/smartcache.
 * If /data/smartcache did not exist, create the volume and mount hcfs on it.
 *
 * @param smartcache_ino Inode number of restored smart cache in restored HCFS.
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t inject_restored_smartcache(ino_t smartcache_ino)
{
	char path_restore[METAPATHLEN];
	char path_nowsys[METAPATHLEN];
	char restored_blockpath[400], thisblockpath[400];
	char block_status;
	ino_t tmp_ino;
	FILE_META_HEADER origin_header, tmp_header;
	FILE *fptr;
	int64_t ret_ssize;
	uint64_t generation;
	int64_t count, total_blocks;
	int32_t ret, errcode;
	BOOL meta_open = FALSE;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	char pin_type;
	struct stat tempstat;
	int64_t blocksize, datasize_est, blocknum_est, restored_smartcache_size;
	char cmd[300];
	FILE_BLOCK_ITERATOR *iter;

	/* Check if vol "hcfs_smartcache" exist. Create and mount if
	 * it did not exist */
	ret = mount_status(SMART_CACHE_VOL_NAME);
	if (ret < 0 && ret != -ENOENT)
		return ret;
	if (data_smart_root == 0 || ret == -ENOENT) {
		DIR_ENTRY tmp_entry;

		if (access(SMART_CACHE_ROOT_MP, F_OK) < 0) {
			if (errno == ENOENT) {
				ret = mkdir(SMART_CACHE_ROOT_MP, 0771);
				if (ret < 0) {
					write_log(0, "Fail to mkdir in %s."
						" Code %d", __func__, errno);
					return -errno;
				}
			} else {
				write_log(0, "Fail to access %s. Code %d",
					SMART_CACHE_ROOT_MP, errno);
				return -errno;
			}
		}
		ret = add_filesystem(SMART_CACHE_VOL_NAME, ANDROID_INTERNAL,
				&tmp_entry);
		if (ret < 0 && ret != -EEXIST) {
			write_log(0, "Error: Fail to add new vol in %s."
					" Code %d", __func__, -ret);
			return ret;
		}
		ret = mount_FS(SMART_CACHE_VOL_NAME, SMART_CACHE_ROOT_MP, 0);
		if (ret < 0) {
			write_log(0, "Error: Fail to mount vol in %s."
					" Code %d", __func__, -ret);
			return ret;
		}

		/* Restore label */
		sprintf(cmd, "restorecon %s", SMART_CACHE_ROOT_MP);
		ret = _run_command(cmd);
		if (ret < 0) {
			write_log(0, "Error: Fail to restorecon in %s."
					" Code %d", __func__, -ret);
			return ret;
		}
		ret = chmod(SMART_CACHE_ROOT_MP, 0771);
		if (ret < 0) {
			write_log(0, "Error: Fail to chmod in %s."
					" Code %d", __func__, -ret);
			return ret;
		}
	}

	sc_data = (RESTORED_SMARTCACHE_DATA *)
			calloc(sizeof(RESTORED_SMARTCACHE_DATA), 1);
	if (!sc_data)
		return -errno;

	fetch_restore_meta_path(path_restore, smartcache_ino);
	fptr = fopen(path_restore, "r+");
	if (!fptr) {
		write_log(0, "Error: Fail to open %s. Code %d",
				path_restore, errno);
		return -errno;
	}
	meta_open = TRUE;
	PREAD(fileno(fptr), &origin_header, sizeof(FILE_META_HEADER), 0);
	memcpy(&tmp_header, &origin_header, sizeof(FILE_META_HEADER));
	tmp_ino = super_block_new_inode(&(tmp_header.st), &generation, P_PIN);

	pin_type = tmp_header.fmt.local_pin;
	iter = init_block_iter(fptr); /* Block iterator */
	if (!iter) {
		fclose(fptr);
		return -errno;
	}
	total_blocks = iter->total_blocks;

	/* Extend CACHE_HARD_LIMIT */
	restored_smartcache_size = tmp_header.st.size;
	sem_wait(&(hcfs_system->access_sem)); /* Lock system meta */
	update_restored_cache_usage(-restored_smartcache_size,
			-total_blocks, pin_type);
	_change_stage1_cache_limit(restored_smartcache_size);
	hcfs_system->systemdata.system_size += restored_smartcache_size;
	hcfs_system->systemdata.pinned_size += restored_smartcache_size;
	hcfs_system->systemdata.cache_size += restored_smartcache_size;
	hcfs_system->systemdata.cache_blocks += total_blocks;
	sem_post(&(hcfs_system->access_sem)); /* Unlock system meta */
	datasize_est = restored_smartcache_size;
	blocknum_est = total_blocks;

	/* Move blocks */
	while (iter_next(iter)) {
		/* Skip if block does not exist */
		block_status = iter->now_bentry->status;
		if (block_status == ST_NONE)
			continue;

		count = iter->now_block_no; /* Block index */
		fetch_restore_block_path(restored_blockpath,
				smartcache_ino, count);
		fetch_block_path(thisblockpath, tmp_ino, count);
		ret = rename(restored_blockpath, thisblockpath);
		if (ret < 0) {
			write_log(0, "Error: Fail to rename in %s. Code %d",
					__func__, -ret);
			errcode = ret;
			goto errcode_handle;
		}
		/* Re-estimate statistics */
		ret = stat(thisblockpath, &tempstat);
		if (ret < 0) {
			errcode = -errno;
			goto errcode_handle;
		}
		blocksize = tempstat.st_blocks * 512;
		datasize_est -= blocksize;
		blocknum_est -= 1;
	}
	if (errno != ENOENT) {
		errcode = -errno;
		goto errcode_handle;
	}
	destroy_block_iter(iter);

	sem_wait(&(hcfs_system->access_sem)); /* Lock system meta */
	hcfs_system->systemdata.cache_size -= datasize_est;
	hcfs_system->systemdata.cache_blocks -= blocknum_est;
	update_restored_cache_usage(datasize_est, blocknum_est, pin_type);
	sem_post(&(hcfs_system->access_sem)); /* Unlock system meta */

	/* Prepare meta */
	tmp_header.st.ino = tmp_ino;
	tmp_header.st.nlink = 1;
	tmp_header.fmt.root_inode = data_smart_root;
	tmp_header.fmt.generation = generation;
	PWRITE(fileno(fptr), &tmp_header, sizeof(FILE_META_HEADER), 0);
	fclose(fptr);
	meta_open = FALSE;

	/* Move meta */
	fetch_meta_path(path_nowsys, tmp_ino);
	ret = rename(path_restore, path_nowsys);
	if (ret < 0) {
		write_log(0, "Error: Fail to rename in %s. Code %d",
				__func__, -ret);
		errcode	= ret;
		goto errcode_handle;
	}

	/* Add entry to parent folder */
	body_ptr = meta_cache_lock_entry(data_smart_root);
	if (!body_ptr) {
		errcode	= -errno;
		goto errcode_handle;
	}
	ret = meta_cache_open_file(body_ptr);
	if (ret < 0) {
		meta_cache_unlock_entry(body_ptr);
		errcode = ret;
		goto errcode_handle;
	}
	/* TODO: check if hcfs_restore exist. It should be remove at the
	 * begining of restoration stage 1 */
	ret = dir_add_entry(data_smart_root, tmp_ino,
			RESTORED_SMARTCACHE_TMP_NAME,
			tmp_header.st.mode, body_ptr, FALSE);
	if (ret < 0) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		errcode = ret;
		goto errcode_handle;
	}
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	DIR_STATS_TYPE tmpstat = {.num_local = 1,
				  .num_cloud = 0,
				  .num_hybrid = 0};
	sem_wait(&(pathlookup_data_lock));
	ret = update_dirstat_parent(data_smart_root, &tmpstat);
	sem_post(&(pathlookup_data_lock));
	if (ret < 0) {
		write_log(0, "Error: Fail to update dir stat. Code %d", -ret);
		errcode = ret;
		goto errcode_handle;
	}

	/* Record data */
	memcpy(&(sc_data->restored_smartcache_header), &origin_header,
			sizeof(FILE_META_HEADER));
	sc_data->inject_smartcache_ino = tmp_ino;
	write_log(4, "Inject smart cache. Inode %"PRIu64, (uint64_t)tmp_ino);
	return 0;

errcode_handle:
	if (meta_open)
		fclose(fptr);
	/* Try to remove those blocks */
	for (count = 0; count < total_blocks; count++) {
		fetch_restore_block_path(restored_blockpath,
				smartcache_ino, count);
		if (access(restored_blockpath, F_OK) == 0)
			unlink(restored_blockpath);
		fetch_block_path(thisblockpath, tmp_ino, count);
		if (access(thisblockpath, F_OK) == 0)
			unlink(thisblockpath);
	}
	FREE(sc_data);
	return errcode;
}

/**
 * Repair restored smart cache "/data/smartcache/hcfsblock_restore" using e2fsck
 * and mount it on folder /data/mnt/hcfsblock_restore/.
 *
 * @return 0 on success, -ECANCELED if critical error occurred.
 */
int32_t mount_and_repair_restored_smartcache()
{
	int32_t ret;
	int32_t errcode;
	char command[COMMAND_LEN];

	/* Setup loop device */
	snprintf(command, COMMAND_LEN, "losetup %s %s/%s",
			RESTORED_SMART_CACHE_LODEV,
			SMART_CACHE_ROOT_MP, RESTORED_SMARTCACHE_TMP_NAME);
	ret = _run_command(command);
	if (ret < 0) {
		errcode = -ECANCELED;
		goto errcode_handle;
	}

	/* Repair smart cache using e2fsck */
	snprintf(command, COMMAND_LEN, "e2fsck -pf %s/%s",
			SMART_CACHE_ROOT_MP, RESTORED_SMARTCACHE_TMP_NAME);
	ret = _run_command(command);
	if (ret < 0) {
		/* If e2fsck failed, then discard the smart cache */
		if (ret == -EPERM) {
			errcode = ret; /* Skip smart cache and keep restoring */
		} else {
			errcode = -ECANCELED; /* restoration fail */
		}
		goto errcode_handle;
	}

	/* Create folder and prepare to mount */
	if (access("/data/mnt", F_OK) < 0) {
		write_log(0, "Fail to access %s. Stop restoring"
				" Code %d", "/data/mnt", errno);
		errcode = -ECANCELED;
		goto errcode_handle;
	}
	if (access(RESTORED_SMART_CACHE_MP, F_OK) < 0) {
		errcode = -errno;
		if (errcode == -ENOENT) {
			write_log(4, "Create folder %s",
					RESTORED_SMART_CACHE_MP);
			MKDIR(RESTORED_SMART_CACHE_MP, 0);
		} else {
			write_log(0, "Fail to access %s. Stop restoring"
				" Code %d", RESTORED_SMART_CACHE_MP, -errcode);
			errcode = -ECANCELED;
			goto errcode_handle;
		}
	}
	/* Mount restored smart cache */
	ret = mount(RESTORED_SMART_CACHE_LODEV, RESTORED_SMART_CACHE_MP,
			"ext4", 0, NULL);
	if (ret < 0) {
		write_log(0, "Error: Fail to mount. Code %d", errno);
		errcode = -ECANCELED;
		goto errcode_handle;
	}

	write_log(4, "Info: Smart cache had been repired and mounted.");
	return 0;

errcode_handle:
	return errcode;
}

/*
 * Remove restored smartcache from now active hcfs under /data/smartcache.
 * This function is invoked after restoration of /data/data completed and then
 * we need to extract restored smart cache from now hcfs system.
 */
static int32_t _remove_from_now_hcfs(ino_t ino_nowsys)
{
	META_CACHE_ENTRY_STRUCT *parent_ptr, *sc_ptr;
	DIR_STATS_TYPE tmpstat;
	int32_t ret;

	parent_ptr = meta_cache_lock_entry(data_smart_root);
	if (!parent_ptr)
		return -errno;
	/* Remove entry */
	ret = dir_remove_entry(data_smart_root, ino_nowsys,
			RESTORED_SMARTCACHE_TMP_NAME, S_IFREG,
			parent_ptr, FALSE);
	meta_cache_close_file(parent_ptr);
	meta_cache_unlock_entry(parent_ptr);
	if (ret < 0) {
		write_log(0, "Error: Fail to remove entry in %s. Code %d",
				__func__, -ret);
		return ret;
	}

	/* TODO: Is statistics ok? */
	sc_ptr = meta_cache_lock_entry(ino_nowsys);
	if (!parent_ptr) {
		write_log(0, "Error: Fail to lock restored smart cache."
				" Code %d", errno);
		return -errno;
	}
	ret = meta_cache_open_file(sc_ptr);
	if (ret < 0) {
		write_log(0, "Error: Fail to open restored smart cache."
				" Code %d", errno);
		return ret;
	}
	ret = check_file_storage_location(sc_ptr->fptr, &tmpstat);
	meta_cache_close_file(sc_ptr);
	meta_cache_unlock_entry(sc_ptr);
	if (ret < 0) {
		return ret;
	}

	/* Delete path lookup table */
	ret = sem_wait(&(pathlookup_data_lock));
	if (ret < 0)
		return ret;
	tmpstat.num_local = -tmpstat.num_local;
	tmpstat.num_cloud = -tmpstat.num_cloud;
	tmpstat.num_hybrid = -tmpstat.num_hybrid;
	ret = update_dirstat_parent(data_smart_root, &tmpstat);
	if (ret < 0) {
		sem_post(&(pathlookup_data_lock));
		return ret;
	}
	ret = lookup_delete_parent(sc_data->inject_smartcache_ino,
			data_smart_root);
	sem_post(&(pathlookup_data_lock));
	if (ret < 0 && ret != -ENOENT) {
		write_log(0, "Error: Fail to delete parent in %s. Code %d",
				__func__, -ret);
		return ret;
	}

	ret = meta_cache_remove(ino_nowsys);
	if (ret < 0 && ret != -ENOENT) {
		write_log(0, "Error: Fail to remove meta cache in %s. Code %d",
				__func__, -ret);
		return ret;
	}

	return 0;
}

/**
 * Extract restored smartcache from now active HCFS to restored meta and data
 * folder, and remove the entry hcfsblock_restore from /data/smartcache. Also
 * recover cache statistics.
 *
 * @param smartcache_ino Inode number of restored smart cache in restored HCFS.
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t extract_restored_smartcache(ino_t smartcache_ino)
{
	char path_restore[METAPATHLEN];
	char path_nowsys[METAPATHLEN];
	char restored_blockpath[400], thisblockpath[400];
	int32_t ret, errcode;
	ino_t ino_nowsys;
	char block_status;
	FILE_META_HEADER tmp_header;
	FILE *fptr;
	int64_t ret_ssize;
	int64_t count, total_blocks;
	BOOL meta_open = FALSE;
	struct stat tempstat;
	int64_t blocksize, datasize_est, blocknum_est, restored_smartcache_size;
	char pin_type;
	FILE_BLOCK_ITERATOR *iter;

	ino_nowsys = sc_data->inject_smartcache_ino;
	ret = _remove_from_now_hcfs(ino_nowsys);
	if (ret < 0) {
		write_log(0, "Error: Fail to remove restored smartcache."
			" Code %d", -ret);
		return ret;
	}

	/* Extract this file */
	fetch_restore_meta_path(path_restore, smartcache_ino);
	fetch_meta_path(path_nowsys, ino_nowsys);
	ret = rename(path_nowsys, path_restore);
	if (ret < 0) {
		write_log(0, "Error: Fail to rename in %s. Code %d",
				__func__, errno);
		return -errno;
	}
	fptr = fopen(path_restore, "r+");
	if (!fptr) {
		write_log(0, "Error: Fail to open. Code %d", errno);
		return -errno;
	}
	meta_open = TRUE;
	PREAD(fileno(fptr), &tmp_header, sizeof(FILE_META_HEADER), 0);
	pin_type = tmp_header.fmt.local_pin;
	iter = init_block_iter(fptr);
	if (!iter) {
		fclose(fptr);
		return -errno;
	}
	total_blocks = iter->total_blocks;

	/* Update statistics */
	restored_smartcache_size = tmp_header.st.size;
	sem_wait(&(hcfs_system->access_sem)); /* Lock system meta */
	hcfs_system->systemdata.system_size -= restored_smartcache_size;
	hcfs_system->systemdata.pinned_size -= restored_smartcache_size;
	hcfs_system->systemdata.cache_size -= restored_smartcache_size;
	hcfs_system->systemdata.cache_blocks -= total_blocks;
	_change_stage1_cache_limit(-restored_smartcache_size);
	update_restored_cache_usage(restored_smartcache_size,
				total_blocks, pin_type);
	sem_post(&(hcfs_system->access_sem)); /* Unlock system meta */

	datasize_est = restored_smartcache_size;
	blocknum_est = total_blocks;

	while (iter_next(iter)) {
		/* Skip if block does not exist */
		block_status = iter->now_bentry->status;
		if (block_status == ST_NONE)
			continue;
		count = iter->now_block_no;
		fetch_restore_block_path(restored_blockpath,
				smartcache_ino, count);
		fetch_block_path(thisblockpath, ino_nowsys, count);
		ret = rename(thisblockpath, restored_blockpath);
		if (ret < 0) {
			write_log(0, "Error: Fail to rename in %s. Code %d",
					__func__, errno);
			errcode = -errno;
			goto errcode_handle;
		}
		/* Statistics */
		ret = stat(restored_blockpath, &tempstat);
		if (ret < 0) {
			errcode = -errno;
			goto errcode_handle;
		}
		blocksize = tempstat.st_blocks * 512;
		datasize_est -= blocksize;
		blocknum_est -= 1;
	}
	if (errno != ENOENT) {
		errcode = -errno;
		goto errcode_handle;
	}
	destroy_block_iter(iter);

	/* Update statistics */
	sem_wait(&(hcfs_system->access_sem)); /* Lock system meta */
	hcfs_system->systemdata.cache_size += datasize_est;
	hcfs_system->systemdata.cache_blocks += blocknum_est;
	update_restored_cache_usage(-datasize_est, -blocknum_est, pin_type);
	sem_post(&(hcfs_system->access_sem)); /* Unlock system meta */

	/* Recover some data */
	tmp_header.st.ino = smartcache_ino;
	tmp_header.st.nlink =
		sc_data->restored_smartcache_header.st.nlink;
	tmp_header.fmt.root_inode =
		sc_data->restored_smartcache_header.fmt.root_inode;
	tmp_header.fmt.generation =
		sc_data->restored_smartcache_header.fmt.generation;
	PWRITE(fileno(fptr), &tmp_header, sizeof(FILE_META_HEADER), 0);
	fclose(fptr);
	meta_open = FALSE;
	FREE(sc_data);

	/* Reclaim the smart cahce inode in now hcfs */
	ret = super_block_to_delete(ino_nowsys, FALSE);
	if (ret < 0)
		return ret;
	ret = super_block_delete(ino_nowsys);
	if (ret < 0)
		return ret;

	write_log(4, "Info: Extracting restored smart cache from"
			" now system completed");
	rmdir(RESTORED_SMART_CACHE_MP); /* Just try to rm the folder */
	return 0;

errcode_handle:
	if (meta_open)
		fclose(fptr);
	/* Try to remove those blocks */
	for (count = 0; count < total_blocks; count++) {
		fetch_restore_block_path(restored_blockpath,
				smartcache_ino, count);
		if (access(restored_blockpath, F_OK) == 0)
			unlink(restored_blockpath);
		fetch_block_path(thisblockpath, ino_nowsys, count);
		if (access(thisblockpath, F_OK) == 0)
			unlink(thisblockpath);
	}
	FREE(sc_data);
	return errcode;
}
