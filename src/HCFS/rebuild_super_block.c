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
	fsmgr_fptr = fopen(fsmgr_path, "r");
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

int32_t init_rebuild_sb()
{
	char sb_path[200];
	int32_t ret, errcode;
	ino_t *roots;
	int64_t num_roots;

	sprintf(sb_path, "%s/superblock", METAPATH);
	if (access(sb_path, F_OK) < 0) {
		errcode = errno;
		if (errcode == ENOENT) {
			/* Get roots */
			ret = _get_root_inodes(&roots, &num_roots);
			if (ret < 0)
				return ret;
			/* TODO:Init head */
		} else {
			return -errcode;
		}
	}
	ret = super_block_init();
	if (ret < 0)
		return ret;

	return 0;
}

int32_t rebuild_sb_manager()
{
	rebuild_sb_mgr_info = (REBUILD_SB_MGR_INFO *)
			calloc(sizeof(REBUILD_SB_MGR_INFO), 1);
	if (!rebuild_sb_mgr_info) {
		write_log(0, "Error: Fail to allocate memory in %s\n",
				__func__);
		return -ENOMEM;
	}

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
