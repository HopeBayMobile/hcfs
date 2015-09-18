#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#include "hcfs_tocloud.h"
#include "atomic_tocloud.h"
#include "macro.h"
#include "global.h"

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE
extern SYSTEM_CONF_STRUCT system_config;

/*
 * Tag inode as uploading or not_uploading in fuse process memory.
 *
 * Main function of communicating with fuse process. This aims to
 * tag or untag the inode is_uploading flag.
 *
 * @return 0 if succeeding in tagging status, otherwise -1 on error.
 */
int tag_status_on_fuse(ino_t this_inode, char status, int fd)
{
	int sockfd;
	int ret, resp;
	struct sockaddr_un addr;
	UPLOADING_COMMUNICATION_DATA data;

	/* Prepare data */
	data.inode = this_inode;
	data.is_uploading = status;
	data.progress_list_fd = fd;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, FUSE_SOCK_PATH);

	ret = connect(sockfd, (struct sockaddr *)&addr,
		sizeof(struct sockaddr_un));

	send(sockfd, &data, sizeof(UPLOADING_COMMUNICATION_DATA), 0);
	recv(sockfd, &resp, sizeof(int), 0);

	if (resp < 0) {
		write_log(0, "Communication error: Response code %d in %s",
			resp, __func__);
		ret = -1;
	} else {
		write_log(10, "Debug: Communicating to fuse success\n");
		ret = 0;
	}

	close(sockfd);
	return ret;
}

int get_progress_info(int fd, long long block_index,
	BLOCK_UPLOADING_STATUS *block_uploading_status)
{
	long long offset;
	int errcode;
	long long ret_ssize;

	offset = sizeof(BLOCK_UPLOADING_STATUS) * block_index;

	flock(fd, LOCK_EX);
	PREAD(fd, block_uploading_status, sizeof(BLOCK_UPLOADING_STATUS),
		offset);
	flock(fd, LOCK_UN);
	if (ret_ssize == 0) {
		/* Reading exceeds EOF. It may occur when backend
		   has truncated data and this block has not been
		   uploaded. */
		memset(block_uploading_status, 0,
			sizeof(BLOCK_UPLOADING_STATUS));
		block_uploading_status->finish_uploading = FALSE;
	}

	return ret_ssize;

errcode_handle:
	write_log(0, "Error: Fail to get progress-info of block_%lld\n",
			block_index);
	return errcode;

}

int set_progress_info(int fd, long long block_index,
	BLOCK_UPLOADING_STATUS *set_block_uploading_status,
	char set_which_one)
{
	int errcode;
	long long offset;
	off_t end_pos;
	ssize_t ret_ssize;
	BLOCK_UPLOADING_STATUS block_uploading_status;

	offset = sizeof(BLOCK_UPLOADING_STATUS) * block_index;

	flock(fd, LOCK_EX);
	PREAD(fd, &block_uploading_status, sizeof(BLOCK_UPLOADING_STATUS),
		offset);
	if (ret_ssize == 0) { /* Init because backend_blocks < now blocks */
		memset(&block_uploading_status, 0,
			sizeof(BLOCK_UPLOADING_STATUS));
		end_pos = lseek(fd, 0, SEEK_END);
		while (end_pos < offset) {
			PWRITE(fd, &block_uploading_status,
				sizeof(BLOCK_UPLOADING_STATUS), end_pos);
			end_pos += sizeof(BLOCK_UPLOADING_STATUS);
		}
		if (end_pos > offset)
			write_log(0, "Error: end_pos != offset?, in %s\n",
				__func__);
	}

	if (set_which_one == TOUPLOAD_BLOCKS) {
		block_uploading_status.block_exist =
			((set_block_uploading_status->block_exist & 1) |
			(block_uploading_status.block_exist & 2));

#ifdef DEDUP_ENABLE
		memcpy(block_uploading_status.to_upload_objid,
			set_block_uploading_status->to_upload_objid,
			sizeof(unsigned char) * OBJID_LENGTH);
#else
		block_uploading_status.toupload_seq =
			set_block_uploading_status->toupload_seq;
#endif

	} else if (set_which_one == BACKEND_BLOCKS) {
		block_uploading_status.block_exist =
			((set_block_uploading_status->block_exist & 2) |
			(block_uploading_status.block_exist & 1));

#ifdef DEDUP_ENABLE
		memcpy(block_uploading_status.backend_objid,
			set_block_uploading_status->backend_objid,
			sizeof(unsigned char) * OBJID_LENGTH);
#else
		block_uploading_status.backend_seq =
			set_block_uploading_status->backend_seq;
#endif
	}

	block_uploading_status.finish_uploading =
		set_block_uploading_status->finish_uploading;

	PWRITE(fd, &block_uploading_status,
		sizeof(BLOCK_UPLOADING_STATUS), offset);
	flock(fd, LOCK_UN);

	if (set_block_uploading_status->finish_uploading == TRUE)
		write_log(10, "Debug: block_%lld finished uploading - "
			"fd = %d\n", block_index, fd);

	return 0;

errcode_handle:
	return errcode;
}

int get_progress_info_nonlock(int fd, long long block_index,
	BLOCK_UPLOADING_STATUS *block_uploading_status)
{
	long long offset;
	int errcode;
	long long ret_ssize;

	offset = sizeof(BLOCK_UPLOADING_STATUS) * block_index;

	PREAD(fd, block_uploading_status, sizeof(BLOCK_UPLOADING_STATUS),
		offset);
	if (ret_ssize == 0) {
		memset(block_uploading_status, 0,
			sizeof(BLOCK_UPLOADING_STATUS));
		block_uploading_status->finish_uploading = FALSE;
	}

	return ret_ssize;

errcode_handle:
	write_log(0, "Error: Fail to get progress-info of block_%lld\n",
			block_index);
	return errcode;

}

int open_progress_info(ino_t inode)
{
	int ret_fd;
	int errcode;
	char filename[200];

	if (access("upload_bullpen", F_OK) == -1)
		mkdir("upload_bullpen", 0700);

	sprintf(filename, "upload_bullpen/upload_progress_inode_%ld",
		inode);
	ret_fd = open(filename, O_CREAT | O_RDWR);
	if (ret_fd < 0) {
		errcode = errno;
		write_log(0, "Error: Fail to open uploading progress file"
			" in %s. Code %d\n", __func__, errcode);
		return ret_fd;
	} else {
		write_log(10, "Debug: Open progress-info file for inode %lld,"
			" fd = %d\n", inode, ret_fd);
	}

	return ret_fd;
}

int init_progress_info(int fd, long long backend_blocks, FILE *backend_metafptr)
{
	int errcode;
	long long offset, ret_ssize;
	BLOCK_UPLOADING_STATUS block_uploading_status;
	struct stat tempstat;
	long long e_index, which_page, current_page, page_pos;
	long long block;
	BLOCK_ENTRY_PAGE block_page;
	FILE_META_TYPE tempfilemeta;
	char cloud_status;

	/* Do NOT need to lock file because this function will be called by
	   the only one thread in sync_single_inode() */
	if (backend_metafptr == NULL) {
		offset = 0;
		memset(&block_uploading_status, 0,
				sizeof(BLOCK_UPLOADING_STATUS));
		for (block = 0; block < backend_blocks; block++) {
			PWRITE(fd, &block_uploading_status,
					sizeof(BLOCK_UPLOADING_STATUS), offset);
			offset += sizeof(BLOCK_UPLOADING_STATUS);
		}

		return 0;
	}

	PREAD(fileno(backend_metafptr), &tempfilemeta, sizeof(FILE_META_TYPE),
							sizeof(struct stat));

	write_log(10, "Debug: backend blocks = %lld\n", backend_blocks);

	/* Write into progress info */
	current_page = -1;
	offset = 0;
	for (block = 0; block < backend_blocks; block++) {
		e_index = block % BLK_INCREMENTS;
		which_page = block / BLK_INCREMENTS;

		if (current_page != which_page) {
			page_pos = seek_page2(&tempfilemeta,
				backend_metafptr, which_page, 0);
			if (page_pos <= 0) {
				block += (BLK_INCREMENTS - 1);
				continue;
			}
			current_page = which_page;
			PREAD(fileno(backend_metafptr), &block_page,
					sizeof(BLOCK_ENTRY_PAGE), page_pos);
		}

		memset(&block_uploading_status, 0,
				sizeof(BLOCK_UPLOADING_STATUS));

		cloud_status = block_page.block_entries[e_index].status;
		if ((cloud_status != ST_NONE) &&
			(cloud_status != ST_TODELETE)) {
			SET_CLOUD_BLOCK_EXIST(
				block_uploading_status.block_exist);
#ifdef DEDUP_ENABLE
			memcpy(block_uploading_status.backend_objid,
				block_page.block_entries[e_index].obj_id,
				sizeof(char) * OBJID_LENGTH);
#else
			block_uploading_status.backend_seq = 0; /* temp */
#endif
		}
		PWRITE(fd, &block_uploading_status,
				sizeof(BLOCK_UPLOADING_STATUS), offset);
		offset += sizeof(BLOCK_UPLOADING_STATUS);
	}

	return 0;

errcode_handle:
	return errcode;
}

int close_progress_info(int fd, ino_t inode)
{
	char filename[200];
	int ret, errcode;

	sprintf(filename, "upload_bullpen/upload_progress_inode_%ld",
		inode);

	close(fd);
	UNLINK(filename);

	write_log(10, "Debug: Close progress-info file for inode %lld\n",
		inode);

	return 0;

errcode_handle:
	return errcode;
}

int fetch_toupload_meta_path(char *pathname, ino_t inode)
{
	int errcode, ret;

	if (access("upload_bullpen", F_OK) == -1)
		MKDIR("upload_bullpen", 0700);

#ifdef ARM_32bit_
	sprintf(pathname, "upload_bullpen/hcfs_local_meta_%lld.tmp", inode);
#else
	sprintf(pathname, "upload_bullpen/hcfs_local_meta_%ld.tmp", inode);
#endif

	return 0;

errcode_handle:
	return errcode;
}

int fetch_toupload_block_path(char *pathname, ino_t inode,
	long long block_no, long long seq)
{

#ifdef ARM_32bit_
	sprintf(pathname, "/dev/shm/hcfs_sync_block_%lld_%lld_%lld.tmp",
		inode, block_no, seq);
#else
	sprintf(pathname, "/dev/shm/hcfs_sync_block_%ld_%lld_%lld.tmp",
		inode, block_no, seq);
#endif

	return 0;
}

int fetch_backend_meta_path(char *pathname, ino_t inode)
{
#ifdef ARM_32bit_
	sprintf(pathname, "upload_bullpen/hcfs_backend_meta_%lld.tmp", inode);
#else
	sprintf(pathname, "upload_bullpen/hcfs_backend_meta_%ld.tmp", inode);
#endif
	return 0;
}

/**
 * Check whether target file exists or not and copy source file.
 *
 * This function first checks whether source file exist and whether target file
 * does not exist and then lock source file and copy it if.
 *
 * @return 0 if succeed in copy, -EEXIST in case of target file existing.
 */
int check_and_copy_file(const char *srcpath, const char *tarpath)
{
	int errcode;
	int ret;
	size_t read_size;
	size_t ret_size;
	ssize_t ret_ssize;
	FILE *src_ptr, *tar_ptr;
	char filebuf[4100];
	long long temp_trunc_size;

	/* source file should exist */
	if (access(srcpath, F_OK) != 0) {
		errcode = errno;
		if (errcode == ENOENT)
			write_log(0, "Error: Source file does not exist. In %s\n",
				__func__);
		else
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
		return -errcode;
	}

	/* if target file exists, do not copy it.
	   (it may be copied by another process) */
	ret = access(tarpath, F_OK);
	if (ret == 0) {
		return -EEXIST;
	} else {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
			return -errcode;
		}
	}

	src_ptr = fopen(srcpath, "r");
	if (src_ptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		return -errcode;
	}

	/* Lock source, do NOT need to lock target because
	   other processes will be locked at source file. */
	flock(fileno(src_ptr), LOCK_EX);

	/* Check again to avoid race condition*/
	if (access(tarpath, F_OK) == 0) {
		flock(fileno(src_ptr), LOCK_UN);
		fclose(src_ptr);
		return -EEXIST;
	}

	if (access(srcpath, F_OK) < 0) {
		errcode = errno;
		flock(fileno(src_ptr), LOCK_UN);
		fclose(src_ptr);
		return -errcode;
	}

	tar_ptr = fopen(tarpath, "w+");
	if (tar_ptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));

		flock(fileno(src_ptr), LOCK_UN);
		fclose(src_ptr);
		fclose(src_ptr);
		return -errcode;
	}

	/* Copy */
	FSEEK(src_ptr, 0, SEEK_SET);
	while (!feof(src_ptr)) {
		FREAD(filebuf, 1, 4096, src_ptr);
		read_size = ret_size;
		if (read_size > 0) {
			FWRITE(filebuf, 1, read_size, tar_ptr);
		} else {
			break;
		}
	}

	/* Copy xattr "trunc_size" if it exists */
	ret_ssize = fgetxattr(fileno(src_ptr), "user.trunc_size",
		&temp_trunc_size, sizeof(long long));
	if (ret_ssize >= 0) {
		fsetxattr(fileno(tar_ptr), "user.trunc_size",
			&temp_trunc_size, sizeof(long long), 0);

		fremovexattr(fileno(src_ptr), "user.trunc_size");
		write_log(10, "Debug: trunc_size = %lld",temp_trunc_size);
	}

	/* Unlock soruce file */
	flock(fileno(src_ptr), LOCK_UN);
	fclose(src_ptr);
	fclose(tar_ptr);

	return 0;

errcode_handle:
	flock(fileno(src_ptr), LOCK_UN);
	fclose(src_ptr);
	fclose(tar_ptr);
	return -errcode;
}

char did_block_finish_uploading(int fd, long long blockno)
{
	int ret;
	BLOCK_UPLOADING_STATUS block_uploading_status;

	ret = get_progress_info(fd, blockno, &block_uploading_status);
	if (ret < 0) {
		write_log(0, "Error: Fail to get progress info\n");
		return FALSE;
	}
	return block_uploading_status.finish_uploading;
}

void _revert_inode_uploading(SYNC_THREAD_TYPE *data_ptr)
{
	char toupload_meta_exist, backend_meta_exist;
	char toupload_meta_path[200];
	char backend_meta_path[200];
	int errcode;
	off_t progress_size;
	mode_t this_mode;
	ino_t inode;
	int progress_fd;
	long long total_blocks;

	this_mode = data_ptr->this_mode;
	inode = data_ptr->inode;
	progress_fd = data_ptr->progress_fd;

	fetch_backend_meta_path(backend_meta_path, inode);
	fetch_toupload_meta_path(toupload_meta_path, inode);

	/* Check backend meta exist */
	if (access(backend_meta_path, F_OK) == 0) {
		backend_meta_exist = TRUE;
	} else {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "Error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
			return;
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
			return;
		} else {
			toupload_meta_exist = FALSE;
		}
	}

	/*** Begin to revert ***/
	progress_size = lseek(progress_fd, 0, SEEK_END);

	if (toupload_meta_exist == TRUE) {
		if ((backend_meta_exist == FALSE) && (progress_size != 0)) {
		/* TODO: Keep on uploading. case[5, 6], case6, case[6, 7],
		case7, case[7, 8], case8 */

		} else { 
		/* NOT begin to upload, so cancel uploading.
		case2, case[2, 3], case3, case[3, 4], case4, case[4, 5], case5,
		 */
			if (backend_meta_exist)
				unlink(backend_meta_path);
			unlink(toupload_meta_path);
		}
	} else {
		if (progress_size == 0) {
		/* Crash before copying local meta, so just 
		cancel uploading. case[1, 2] */
			if (backend_meta_exist)
				unlink(backend_meta_path);

		} else {
		/* Finish uploading all blocks and meta,
		remove backend old block. case[8, 9], case9, case[9. 10],
		case10 */
			total_blocks = progress_size / 
				sizeof(BLOCK_UPLOADING_STATUS);
			delete_backend_blocks(progress_fd, total_blocks,
				inode, BACKEND_BLOCKS);
		}
	}

	return;
}

int uploading_revert()
{	
	DIR *dirptr;
	int fd;
	struct dirent temp_dirent;
	struct dirent *direntptr;
	char upload_pathname[100];
	char progress_filepath[300];
	int errcode, ret, count;
	ino_t inode;
	SYNC_THREAD_TYPE sync_threads[MAX_SYNC_CONCURRENCY];

	sprintf(upload_pathname, "upload_bullpen");
	if (access(upload_pathname, F_OK) < 0)
		return 0;

	dirptr = opendir(upload_pathname);
	if (dirptr == NULL) {
		errcode = errno;
		write_log(0, "Fail to open %s. Code %d, %s\n",
			upload_pathname, errcode, strerror(errcode));
		return -errcode;
	}

	ret = readdir_r(dirptr, &temp_dirent, &direntptr);
	if (ret > 0) {
		errcode = errno;
		write_log(0, "Fail to read %s. Code %d, %s\n",
			upload_pathname, errcode, strerror(errcode));
		closedir(dirptr);
		return -errcode;
	}

	errcode = 0;
	while (direntptr) {
		if (strncmp("upload_progress", temp_dirent.d_name, 15) == 0) {
			ret = sscanf(temp_dirent.d_name,
				"upload_progress_inode_%ld", &inode);
			sprintf(progress_filepath, "%s/%s", upload_pathname,
				temp_dirent.d_name);
			fd = open(progress_filepath, O_RDWR);
			if ((ret != 1) || (fd <= 0)) {
				ret = readdir_r(dirptr, &temp_dirent,
						&direntptr);
				if (ret > 0) {
					errcode = errno;
					break;
				}
				continue;
			}

			sem_wait(&(sync_ctl.sync_queue_sem));
			sem_wait(&(sync_ctl.sync_op_sem));
			for (count = 0; count < MAX_SYNC_CONCURRENCY; 
								count++) {
				if (sync_ctl.threads_in_use[count] == 0)
					break;
			}
			sync_ctl.threads_in_use[count] = inode;
			sync_ctl.threads_created[count] = FALSE;
			sync_ctl.threads_error[count] = FALSE;
			sync_ctl.progress_fd[count] = fd;
			sync_threads[count].inode = inode;
			sync_threads[count].this_mode = 0; // temp
			sync_threads[count].progress_fd = fd;
			pthread_create(&(sync_ctl.inode_sync_thread[count]),
					NULL, (void *)&_revert_inode_uploading,
					(void *)&(sync_threads[count]));
			sync_ctl.threads_created[count] = TRUE;
			sync_ctl.total_active_sync_threads++;

			sem_post(&(sync_ctl.sync_op_sem));
		} else {
			ret = readdir_r(dirptr, &temp_dirent,
					&direntptr);
			if (ret > 0) {
				errcode = errno;
				break;
			}
		}
	}

	closedir(dirptr);
	if (errcode > 0) {
		write_log(0, "Fail to traverse dir %s. Code %d, %s\n",
			upload_pathname, errcode , strerror(errcode));
		return -errcode;
	}

	return 0;
}
