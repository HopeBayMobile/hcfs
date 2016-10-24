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

#include "macro.h"
#include "logger.h"
#include "fuseop.h"
#include "do_restoration.h"
#include "meta.h"
#include "meta_mem_cache.h"
#include "super_block.h"
#include "metaops.h"

int32_t unmount_smart_cache()
{
	int32_t ret;

	ret = umount(SMART_CACHE_PATH);
	if (ret < 0)
		ret = -errno;

	return ret;
}

int32_t mount_smart_cache()
{
	int32_t ret;
	int32_t errcode;

	if (access(SMART_CACHE_PATH, F_OK) < 0) {
		errcode = -errno;
		if (errcode == -ENOENT) {
			write_log(6, "Create folder %s", SMART_CACHE_PATH);
			MKDIR(SMART_CACHE_PATH, 0);
		} else {
			goto errcode_handle;
		}
	}

	ret = mount("/dev/block/loop6", SMART_CACHE_PATH, "ext4", 0, NULL);
	if (ret < 0)
		ret = -errno;

	return ret;

errcode_handle:
	return errcode;
}

int32_t inject_restored_smartcache(ino_t smartcache_ino)
{
	char path[METAPATHLEN];
	char path_nowsys[METAPATHLEN];
	char restored_blockpath[400], thisblockpath[400];
	char block_status;
	ino_t tmp_ino;
	FILE_META_HEADER origin_header, tmp_header;
	FILE_META_TYPE *file_meta;
	FILE *fptr;
	int64_t ret_size, ret_ssize;
	uint64_t generation;
	int64_t count, total_blocks, current_page, which_page, page_pos;
	int32_t e_index;
	int32_t ret, errcode;
	BOOL meta_open = FALSE;
	BLOCK_ENTRY_PAGE tmppage;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	sc_data = (RESTORED_SMARTCACHE_DATA *)
			calloc(sizeof(RESTORED_SMARTCACHE_DATA), 1);
	if (!sc_data)
		return -ENOMEM;

	fetch_restored_meta_path(path, smartcache_ino);
	fptr = fopen(path, "r+");
	if (!fptr) {
		write_log(0, "Error: Fail to open. Code %d", errno);
		return -errno;
	}
	meta_open = TRUE;
	PREAD(fileno(fptr), &origin_header, sizeof(FILE_META_HEADER), 0);
	memcpy(&tmp_header, &origin_header, sizeof(FILE_META_HEADER));
	tmp_ino = super_block_new_inode(&(tmp_header.st), &generation, P_PIN);

	/* Move blocks */
	file_meta = &(tmp_header.fmt);
	total_blocks = tmp_header.st.size == 0 ? 0 :
			((tmp_header.st.size - 1) / MAX_BLOCK_SIZE + 1);
	current_page = -1;
	for (count = 0; count < total_blocks; count++) {
		e_index = count % MAX_BLOCK_ENTRIES_PER_PAGE;
		which_page = count / MAX_BLOCK_ENTRIES_PER_PAGE;

		if (current_page != which_page) {
			page_pos = seek_page2(file_meta, fptr,
					which_page, 0);
			if (page_pos <= 0) {
				count += (MAX_BLOCK_ENTRIES_PER_PAGE - 1);
				continue;
			}
			current_page = which_page;
			FSEEK(fptr, page_pos, SEEK_SET);
			FREAD(&tmppage, sizeof(BLOCK_ENTRY_PAGE),
					1, fptr);
		}

		/* Skip if block does not exist */
		block_status = tmppage.block_entries[e_index].status;
		if (block_status == ST_NONE)
			continue;
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
		/* TODO: Statistics */
	}
	tmp_header.st.ino = tmp_ino;
	tmp_header.st.nlink = 1;
	tmp_header.fmt.root_inode = data_smart_root;
	tmp_header.fmt.generation = generation;
	PWRITE(fileno(fptr), &tmp_header, sizeof(FILE_META_HEADER), 0);
	fclose(fptr);
	meta_open = FALSE;

	/* Move meta */
	fetch_meta_path(path_nowsys, tmp_ino);
	ret = rename(path, path_nowsys);
	if (ret < 0) {
		write_log(0, "Error: Fail to rename in %s. Code %d",
				__func__, -ret);
		errcode	= ret;
		goto errcode_handle;
	}

	/* Add entry to parent folder */
	body_ptr = meta_cache_lock_entry(data_smart_root);
	if (!body_ptr) {
		errcode	= -ENOMEM;
		goto errcode_handle;
	}
	ret = meta_cache_open_file(body_ptr);
	if (ret < 0) {
		meta_cache_unlock_entry(body_ptr);
		errcode = ret;
		goto errcode_handle;
	}
	ret = dir_add_entry(data_smart_root, tmp_ino, SMARTCACHE_TMP_NAME,
			tmp_header.st.mode, body_ptr, FALSE);
	if (ret < 0) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		errcode = ret;
		goto errcode_handle;
	}
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	/* Record data */
	memcpy(&(sc_data->restored_smartcache_header), &origin_header,
			sizeof(FILE_META_HEADER));
	sc_data->inject_smartcache_ino = tmp_ino;
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
