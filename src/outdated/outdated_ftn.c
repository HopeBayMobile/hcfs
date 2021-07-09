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

/**
 * Revert uploading for given inode
 *
 * Following are some crash points:
 * 1. open progress info file
 * 2. copy from local meta to to-upload meta
 *    - Communicate to fuse process and tag inode as uploading
 * 3. download backend meta
 * 4. init all backend block seq or obj-id
 * 5. unlink downloaded meta
 * -------------------------- Continue uploading after finish 5.
 * 6. upload blocks
 * 7. upload to-upload meta
 * 8. unlink to-upload meta
 * 9. delete all backend old blocks
 * 10. close progress info file
 *
 */
void continue_inode_upload(SYNC_THREAD_TYPE *data_ptr)
{
	char toupload_meta_exist, backend_meta_exist, local_meta_exist;
	char toupload_meta_path[200];
	char backend_meta_path[200];
	char local_meta_path[200];
	int32_t errcode;
	mode_t this_mode;
	ino_t inode;
	int32_t progress_fd;
	int64_t total_backend_blocks, total_toupload_blocks;
	ssize_t ret_ssize;
	char finish_init;
	int32_t ret;
	PROGRESS_META progress_meta;

	finish_init = FALSE;
	this_mode = data_ptr->this_mode;
	inode = data_ptr->inode;
	progress_fd = data_ptr->progress_fd;

	fetch_backend_meta_path(backend_meta_path, inode);
	fetch_toupload_meta_path(toupload_meta_path, inode);
	fetch_meta_path(local_meta_path, inode);

	write_log(10, "Debug: Now begin to revert uploading inode_%"PRIu64"\n",
			(uint64_t)inode);
	/* Check backend meta exist */
	if (access(backend_meta_path, F_OK) == 0) {
		backend_meta_exist = TRUE;
	} else {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "Error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
			goto errcode_handle;
		} else {
			backend_meta_exist = FALSE;
		}
	}

	/* Check to-upload meta exist */
	if (access(toupload_meta_path, F_OK) == 0) {
		toupload_meta_exist = TRUE;
	} else {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "Error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
			goto errcode_handle;
		} else {
			toupload_meta_exist = FALSE;
		}
	}

	/* Check local meta */
	if (access(local_meta_path, F_OK) == 0) {
		local_meta_exist = TRUE;
	} else {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "Error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
			goto errcode_handle;
		} else {
			local_meta_exist = FALSE;
		}
	}

	/* If it is not regfile (strange), then just remove all and upload
	 * it again. */
	if (!S_ISREG(this_mode)) {
		if (toupload_meta_exist == TRUE)
			UNLINK(toupload_meta_path);
		if (backend_meta_exist == TRUE)
			UNLINK(backend_meta_path);
		sync_ctl.threads_error[data_ptr->which_index] = TRUE;
		sync_ctl.threads_finished[data_ptr->which_index] = TRUE;
		return;
	}

	PREAD(progress_fd, &progress_meta, sizeof(PROGRESS_META), 0);
	if (progress_meta.now_action != PREPARING) {
		total_backend_blocks = progress_meta.total_backend_blocks;
		total_toupload_blocks = progress_meta.total_toupload_blocks;
		finish_init = TRUE;
	} else {
		total_backend_blocks = 0;
		total_toupload_blocks = 0;
		finish_init = FALSE;
	}

	/*** Begin to check break point ***/
	if (toupload_meta_exist == TRUE) {
		if ((backend_meta_exist == FALSE) && (finish_init == TRUE)) {
		/* Keep on uploading. case[5, 6], case6, case[6, 7],
		case7, case[7, 8], case8 */
			if (local_meta_exist) {
				write_log(10, "Debug: begin continue uploading"
					" inode %"PRIu64"\n", (uint64_t)inode);
				sync_single_inode((void *)data_ptr);
				return;
			} else {
				delete_backend_blocks(progress_fd,
					total_toupload_blocks, inode,
					DEL_TOUPLOAD_BLOCKS);
				sync_ctl.threads_finished[data_ptr->which_index]
				       = TRUE;
				return;	
			}

		} else {
		/* NOT begin to upload, so cancel uploading.
		case2, case[2, 3], case3, case[3, 4], case4, case[4, 5], case5,
		 */
			if (backend_meta_exist)
				unlink(backend_meta_path);
			unlink(toupload_meta_path);
		}
	} else {
		if (finish_init == TRUE) {
		/* Finish uploading all blocks and meta,
		remove backend old block. case[8, 9], case9, case[9. 10],
		case10. Do not need to update backend size again. */
			delete_backend_blocks(progress_fd, total_backend_blocks,
				inode, DEL_BACKEND_BLOCKS);
			sync_ctl.threads_finished[data_ptr->which_index] = TRUE;
			return;
		} else {
		/* Crash before copying local meta, so just
		cancel uploading. case[1, 2] */
			if (backend_meta_exist)
				unlink(backend_meta_path);
		}

	}

	sync_ctl.threads_error[data_ptr->which_index] = TRUE;
	sync_ctl.threads_finished[data_ptr->which_index] = TRUE;
	return;

errcode_handle:
	write_log(0, "Error: Fail to revert/continue uploading inode %"PRIu64"\n",
			(uint64_t)inode);
	sync_ctl.threads_error[data_ptr->which_index] = TRUE;
	sync_ctl.threads_finished[data_ptr->which_index] = TRUE;
	return;
}


