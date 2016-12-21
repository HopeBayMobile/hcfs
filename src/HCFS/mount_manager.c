/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: FS_manager.c
* Abstract: The C source file for mount manager
*
* Revision History
* 2015/7/7 Jiahong created this file
* 2015/7/15 Jiahong adding content
* 2016/1/15 Jiahong changed mount to premount first then mount
*
**************************************************************************/
#define FUSE_USE_VERSION 29

#include "mount_manager.h"

#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <inttypes.h>

#include "fuseop.h"
#include "global.h"
#include "FS_manager.h"
#include "utils.h"
#include "lookup_count.h"
#include "logger.h"
#include "macro.h"
#include "hcfs_fromcloud.h"
#include "meta.h"
#ifdef _ANDROID_ENV_
#include "path_reconstruct.h"
#endif
#include "rebuild_super_block.h"
#include "metaops.h"

MOUNT_T_GLOBAL mount_global = {{0}};

/************************************************************************
*
* Function name: search_mount
*        Inputs: char *fsname, MOUNT_T *mt_info
*       Summary: Search "fsname" in the mount database. If found, returns
*                the mount info in "mt_info"
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t search_mount(char *fsname, char *mp, MOUNT_T **mt_info)
{
	int32_t ret, errcode;
	MOUNT_NODE_T *root;

	root = mount_mgr.root;
	if (root == NULL) {
		errcode = -ENOENT;
		goto errcode_handle;
	}

	if (fsname == NULL) { /* mp can be NULL when just search fsname */
		errcode = -EINVAL;
		goto errcode_handle;
	}

	ret = search_mount_node(fsname, mp, mount_mgr.root, mt_info);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}


	return 0;

errcode_handle:
	return errcode;
}

static int32_t _compare_node(const char *fsname, const char *mp,
		const MOUNT_NODE_T *now_node)
{
	int32_t ret;

	ret = strcmp(fsname, now_node->mt_entry->f_name);
	if (ret == 0) {
		if (mp == NULL) /* Perhaps do not need to compare it
				   when just checking if volume
				   exist or delete all volume. */
			ret = 0;
		else
			if (now_node->mt_entry->f_mp)
				ret = strcmp(mp, now_node->mt_entry->f_mp);
	}

	return ret;
}

/* Helper function for searching binary tree */
int32_t search_mount_node(char *fsname, char *mp, MOUNT_NODE_T *node,
		MOUNT_T **mt_info)
{
	int32_t ret;
	MOUNT_NODE_T *next;

	ret = _compare_node(fsname, mp, node);
	if (ret == 0) {
		/* Found the entry */
		/* Returns not found if being unmounted. */
		if ((node->mt_entry)->is_unmount == TRUE)
			return -ENOENT;
		*mt_info = node->mt_entry;
		return 0;
	}
	if (ret > 0) {
		/* Search the right sub-tree */
		next = node->rchild;
		if (next == NULL)
			return -ENOENT;
		return search_mount_node(fsname, mp, next, mt_info);
	}

	/* Search the left sub-tree */
	next = node->lchild;
	if (next == NULL)
		return -ENOENT;
	return search_mount_node(fsname, mp, next, mt_info);
}

/************************************************************************
*
* Function name: insert_mount
*        Inputs: char *fsname, MOUNT_T *mt_info
*       Summary: Insert "fsname" in the mount database. "mt_info" is the
*                mount info to be stored in the mount database.
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t insert_mount(char *fsname, MOUNT_T *mt_info)
{
	int32_t ret, errcode;
	MOUNT_NODE_T *root;

	write_log(10, "Inserting FS %s\n", fsname);
	root = mount_mgr.root;
	if (root == NULL) {
		/* Add to root */
		write_log(10, "Creating new mount database\n");
		mount_mgr.root = malloc(sizeof(MOUNT_NODE_T));
		if (mount_mgr.root == NULL) {
			errcode = -ENOMEM;
			write_log(0, "Out of memory in %s\n", __func__);
			goto errcode_handle;
		}
		(mount_mgr.root)->lchild = NULL;
		(mount_mgr.root)->rchild = NULL;
		(mount_mgr.root)->parent = NULL;
		(mount_mgr.root)->mt_entry = mt_info;
		mount_mgr.num_mt_FS++;
		return 0;
	}

	ret = insert_mount_node(fsname, mount_mgr.root, mt_info);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	return 0;

errcode_handle:
	return errcode;
}

/* Helper function for inserting to binary tree */
int32_t insert_mount_node(char *fsname, MOUNT_NODE_T *node, MOUNT_T *mt_info)
{
	int32_t ret;
	MOUNT_NODE_T *next;

	ret = _compare_node(fsname, mt_info->f_mp, node);

	if (ret == 0) {
		/* Found the entry */
		return -EEXIST;
	}
	if (ret > 0) {
		/* Search the right sub-tree */
		next = node->rchild;
		if (next == NULL) {
			/* insert to right child */
			node->rchild = malloc(sizeof(MOUNT_NODE_T));
			if (node->rchild == NULL) {
				write_log(0, "Out of memory in %s\n", __func__);
				return -ENOMEM;
			}
			(node->rchild)->lchild = NULL;
			(node->rchild)->rchild = NULL;
			(node->rchild)->parent = node;
			(node->rchild)->mt_entry = mt_info;
			mount_mgr.num_mt_FS++;
			return 0;
		}

		return insert_mount_node(fsname, next, mt_info);
	}

	/* Search the left sub-tree */
	next = node->lchild;
	if (next == NULL) {
		/* insert to left child */
		node->lchild = malloc(sizeof(MOUNT_NODE_T));
		if (node->lchild == NULL) {
			write_log(0, "Out of memory in %s\n", __func__);
			return -ENOMEM;
		}
		(node->lchild)->lchild = NULL;
		(node->lchild)->rchild = NULL;
		(node->lchild)->parent = node;
		(node->lchild)->mt_entry = mt_info;
		mount_mgr.num_mt_FS++;
		return 0;
	}
	return insert_mount_node(fsname, next, mt_info);
}

/* For delete from tree, the tree node will not be freed immediately. This
should be handled in the unmount routines */

/************************************************************************
*
* Function name: delete_mount
*        Inputs: char *fsname, MOUNT_NODE_T **node
*       Summary: Delete "fsname" in the mount database. If deleted, the node
*                is not freed immediately, but will be returned via "node".
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t delete_mount(char *fsname, char *mp, MOUNT_NODE_T **ret_node)
{
	int32_t ret, errcode;
	MOUNT_NODE_T *root;

	root = mount_mgr.root;
	if (root == NULL) {
		/* Nothing to delete */
		errcode = -ENOENT;
		goto errcode_handle;
	}

	if (fsname == NULL) {
		errcode = -EINVAL;
		goto errcode_handle;
	}

	ret = delete_mount_node(fsname, mp, mount_mgr.root, ret_node);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	return 0;

errcode_handle:
	return errcode;
}

int32_t replace_with_largest_child(MOUNT_NODE_T *node, MOUNT_NODE_T **ret_node)
{
	if (node->rchild == NULL) {

		*ret_node = node;
		/* If no children, update parent and return */
		if (node->lchild == NULL) {
			if (((node->parent)->lchild) == node) {
				(node->parent)->lchild = NULL;
			} else if (((node->parent)->rchild) == node) {
				(node->parent)->rchild = NULL;
			} else {
				write_log(0, "Error in mount manager\n");
				return -EIO;
			}
			return 0;
		}

		/* Replace self with left subtree */
		node->lchild->parent = node->parent;
		if (((node->parent)->lchild) == node) {
			(node->parent)->lchild = node->lchild;
		} else if (((node->parent)->rchild) == node) {
			(node->parent)->rchild = node->lchild;
		} else {
			write_log(0, "Error in mount manager\n");
			return -EIO;
		}
		return 0;
	}

	return replace_with_largest_child(node->rchild, ret_node);
}

int32_t delete_mount_node(char *fsname, char *mp, MOUNT_NODE_T *node,
					MOUNT_NODE_T **ret_node)
{
	int32_t ret;
	MOUNT_NODE_T *next;
	MOUNT_NODE_T *ret_child;

	ret = _compare_node(fsname, mp, node);
	/* Check if this is the one */
	if (ret == 0) {
		mount_mgr.num_mt_FS--;
		*ret_node = node;
		/* If no children, update parent and return */
		if ((node->lchild == NULL) && (node->rchild == NULL)) {
			if (node->parent == NULL) {
				/* This is the root */
				mount_mgr.root = NULL;
				return 0;
			}
			if (((node->parent)->lchild) == node) {
				(node->parent)->lchild = NULL;
			} else if (((node->parent)->rchild) == node) {
				(node->parent)->rchild = NULL;
			} else {
				write_log(0, "Error in mount manager\n");
				return -EIO;
			}
			return 0;
		}

		if (node->lchild == NULL) {
			/* Replace self with right subtree */
			node->rchild->parent = node->parent;
			if (node->parent == NULL) {
				/* This is the root */
				mount_mgr.root = node->rchild;
				return 0;
			}
			if (((node->parent)->lchild) == node) {
				(node->parent)->lchild = node->rchild;
			} else if (((node->parent)->rchild) == node) {
				(node->parent)->rchild = node->rchild;
			} else {
				write_log(0, "Error in mount manager\n");
				return -EIO;
			}
			return 0;
		}

		if (node->rchild == NULL) {
			/* Replace self with left subtree */
			node->lchild->parent = node->parent;
			if (node->parent == NULL) {
				/* This is the root */
				mount_mgr.root = node->lchild;
				return 0;
			}
			if (((node->parent)->lchild) == node) {
				(node->parent)->lchild = node->lchild;
			} else if (((node->parent)->rchild) == node) {
				(node->parent)->rchild = node->lchild;
			} else {
				write_log(0, "Error in mount manager\n");
				return -EIO;
			}
			return 0;
		}


		/* Delete this, but need to find the largest child to
			replace */
		ret = replace_with_largest_child(node->lchild, &ret_child);
		if (ret < 0)
			return ret;
		write_log(10, "Replacing node %s\n", node->mt_entry->f_name);
		if (node->lchild != NULL)
			node->lchild->parent = ret_child;
		if (node->rchild != NULL)
			node->rchild->parent = ret_child;
		ret_child->lchild = node->lchild;
		ret_child->rchild = node->rchild;
		ret_child->parent = node->parent;
		if (node->parent == NULL) {
			/* This is the root */
			mount_mgr.root = ret_child;
			return 0;
		}
		if (((node->parent)->lchild) == node) {
			(node->parent)->lchild = ret_child;
		} else if (((node->parent)->rchild) == node) {
			(node->parent)->rchild = ret_child;
		} else {
			write_log(0, "Error in mount manager\n");
			return -EIO;
		}

		return 0;
	}

	if (ret > 0) {
		/* Search the right sub-tree */
		next = node->rchild;
		if (next == NULL)
			return -ENOENT;
		return delete_mount_node(fsname, mp, next, ret_node);
	}

	/* Search the left sub-tree */
	next = node->lchild;
	if (next == NULL)
		return -ENOENT;
	return delete_mount_node(fsname, mp, next, ret_node);
}

/* Routines should also lock FS manager if needed to prevent inconsistency
in the two modules */

/************************************************************************
*
* Function name: init_mount_mgr
*        Inputs: None
*       Summary: Initialize mount manager.
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t init_mount_mgr(void)
{
	memset(&mount_mgr, 0, sizeof(MOUNT_MGR_T));
	sem_init(&(mount_mgr.mount_lock), 0, 1);
	return 0;
}

/************************************************************************
*
* Function name: destroy_mount_mgr
*        Inputs: None
*       Summary: Destroys mount manager. Will call unmount_all to unmount
*                all mounted FS.
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t destroy_mount_mgr(void)
{
	int32_t ret;

	ret = unmount_all();
	/* All mounted FS should be processed. */

	return ret;
}

/* Returns 0 if entry found, and -ENOENT is not found. Otherwise
returns error number */
int32_t FS_is_mounted(char *fsname)
{
	int32_t ret;
	MOUNT_T *tmpinfo;

	/* This volume is mounted now if there is at least one mp. */
	ret = search_mount(fsname, NULL, &tmpinfo);

	return ret;
}

int32_t do_unmount_FS(MOUNT_T *mount_info);
/* Helper for mounting */
int32_t do_mount_FS(char *mp, MOUNT_T *new_info)
{
	struct fuse_chan *tmp_channel;
	struct fuse_session *tmp_session;
	char *mount;
	int32_t mt, fg;

#ifdef _ANDROID_PREMOUNT_
	struct fuse_chan *tmp_ch1;
	int32_t need_unmount;
	need_unmount = FALSE;
#endif

	struct fuse_args tmp_args = FUSE_ARGS_INIT(global_argc, global_argv);

	memcpy(&(new_info->mount_args), &tmp_args, sizeof(struct fuse_args));

	fuse_parse_cmdline(&(new_info->mount_args), &mount, &mt, &fg);

	/* f_ino, f_name, f_mp are filled before calling this function */
	/* global_fuse_args is stored in fuseop.h */

#ifdef _ANDROID_PREMOUNT_
	/* Now changed to premount first if in Android */
	tmp_channel = fuse_premount(mp, &(new_info->mount_args));
#else
	tmp_channel = fuse_mount(mp, &(new_info->mount_args));
#endif
	if (tmp_channel == NULL) {
		write_log(10, "Unable to create channel in mounting\n");
		goto errcode_handle;
	}
	tmp_session = fuse_lowlevel_new(&(new_info->mount_args),
			&hfuse_ops, sizeof(hfuse_ops), (void *) new_info);
	if (tmp_session == NULL) {
		write_log(10, "Unable to create session in mounting\n");
		goto errcode_handle;
	}
	fuse_session_add_chan(tmp_session, tmp_channel);
	gettimeofday(&(new_info->mt_time), NULL);
	new_info->session_ptr = tmp_session;
	new_info->chan_ptr = tmp_channel;
	if (new_info->mp_mode == MP_DEFAULT)
		mount_global.ch[MP_DEFAULT] = tmp_channel;
	if (new_info->mp_mode == MP_READ)
		mount_global.ch[MP_READ] = tmp_channel;
	if (new_info->mp_mode == MP_WRITE)
		mount_global.ch[MP_WRITE] = tmp_channel;
	new_info->is_unmount = FALSE;
	if (mt == TRUE)
		pthread_create(&(new_info->mt_thread), NULL,
			mount_multi_thread, (void *)new_info);
	else
		pthread_create(&(new_info->mt_thread), NULL,
			mount_single_thread, (void *)new_info);
#ifdef _ANDROID_PREMOUNT_
	need_unmount = TRUE;
	memcpy(&(new_info->mount_args), &tmp_args, sizeof(struct fuse_args));
        tmp_ch1 = fuse_mount(mp, &(new_info->mount_args), tmp_channel);
        if (tmp_ch1 == NULL) {
                write_log(10, "Unable to mount\n");
                goto errcode_handle;
        }
#endif

	return 0;
errcode_handle:
	write_log(2, "Error in mounting %s\n", new_info->f_name);
	if (tmp_channel != NULL)
		fuse_unmount(mp, tmp_channel);
#ifdef _ANDROID_PREMOUNT_
	if (need_unmount) {
		fuse_set_signal_handlers(new_info->session_ptr);
		pthread_kill(new_info->mt_thread, SIGHUP);

		do_unmount_FS(new_info);
	}
#endif

	return -EACCES;
}

/* Helper for unmounting */
int32_t do_unmount_FS(MOUNT_T *mount_info)
{
	sem_wait((mount_info->stat_lock));
	update_FS_statistics(mount_info);
	sem_post((mount_info->stat_lock));
	pthread_join(mount_info->mt_thread, NULL);
	fuse_remove_signal_handlers(mount_info->session_ptr);
	fuse_session_remove_chan(mount_info->chan_ptr);
	fuse_session_destroy(mount_info->session_ptr);
	fuse_unmount(mount_info->f_mp, mount_info->chan_ptr);
	fuse_opt_free_args(&(mount_info->mount_args));

	return 0;
}

/************************************************************************
*
* Function name: mount_FS
*        Inputs: char *fsname, char *mp
*       Summary: Mount filesystem "fsname" to the mountpoint "mp"
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t mount_FS(char *fsname, char *mp, char mp_mode)
{
	int32_t ret, errcode;
	MOUNT_T *tmp_info, *new_info;
	DIR_ENTRY tmp_entry;
	char temppath[METAPATHLEN];

	sem_wait(&(fs_mgr_head->op_lock));
	sem_wait(&(mount_mgr.mount_lock));

#ifndef _ANDROID_ENV_
	UNUSED(mp_mode);
#endif

	/* TODO: check if mp is a valid mountpoint */

	if (fsname == NULL || mp == NULL)
		return -EINVAL;

	/* First check if FS already mounted */
	new_info = NULL;

#ifdef _ANDROID_ENV_
	ret = search_mount(fsname, mp, &tmp_info);
#else
	ret = search_mount(fsname, NULL, &tmp_info); /*Only one mp is allowed*/
#endif
	if (ret != -ENOENT) {
		if (ret == 0) {
			write_log(2, "Mount error: FS already mounted\n");
			errcode = -EPERM;
		} else {
			errcode = ret;
		}
		goto errcode_handle;
	}

	/* Check whether the filesystem exists */
	ret = check_filesystem_core(fsname, &tmp_entry);
	if (ret < 0) {
		if (ret == -ENOENT)
			write_log(2, "Mount error: FS does not exist\n");
		else
			write_log(0, "Unexpected error in mounting.\n");
		errcode = ret;
		goto errcode_handle;
	}

	new_info = malloc(sizeof(MOUNT_T));
	if (new_info == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of memory in %s\n", __func__);
		goto errcode_handle;
	}
	memset(new_info, 0, sizeof(MOUNT_T));

#ifdef _ANDROID_ENV_
	/* Check whether this volume was mounted at other mount points */
	ret = search_mount(fsname, NULL, &tmp_info);
	if (ret == 0) {
		/* Now only ANDROID_MULTIEXTERNAL is allowed to mount at many
		 * mount points */
		if (tmp_info->volume_type != ANDROID_MULTIEXTERNAL) {
			write_log(2, "Mount error: FS already mounted\n");
			errcode = -EPERM;
			goto errcode_handle;
		}

		/* Copy static data */
		new_info->f_ino = tmp_info->f_ino;
		new_info->volume_type = tmp_info->volume_type;
		strcpy(new_info->f_name, tmp_info->f_name);

		/* Shared data */
		new_info->lookup_table = tmp_info->lookup_table;
		new_info->FS_stat = tmp_info->FS_stat;
		new_info->stat_fptr = tmp_info->stat_fptr;
		new_info->stat_lock = tmp_info->stat_lock;
		new_info->vol_path_cache = tmp_info->vol_path_cache;

		/* Self data */
		new_info->mp_mode = mp_mode;
		new_info->f_mp = malloc((sizeof(char) * strlen(mp)) + 10);
		if (new_info->f_mp == NULL) {
			errcode = -ENOMEM;
			write_log(0, "Out of memory in %s\n", __func__);
			goto errcode_handle;
		}
		strcpy((new_info->f_mp), mp);

		ret = do_mount_FS(mp, new_info);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}
		ret = insert_mount(fsname, new_info);
		if (ret < 0) {
			write_log(0, "Unexpected error in mounting.\n");
			write_log(0, "Please unmount manually\n");
			errcode = ret;
			goto errcode_handle;
		}

		sem_post(&(mount_mgr.mount_lock));
		sem_post(&(fs_mgr_head->op_lock));
		return 0;
	}
#endif

	new_info->stat_fptr = NULL;
	new_info->f_ino = tmp_entry.d_ino;

#ifdef _ANDROID_ENV_
	new_info->mp_mode = mp_mode;
	new_info->volume_type = tmp_entry.d_type;
	if (new_info->volume_type == ANDROID_EXTERNAL ||
	    new_info->volume_type == ANDROID_MULTIEXTERNAL) {
		new_info->vol_path_cache = init_pathcache(new_info->f_ino);
		if (new_info->vol_path_cache == NULL) {
			errcode = -ENOMEM;
			goto errcode_handle;
		}
	} else {
		new_info->vol_path_cache = NULL;
	}
#endif

	/* Try fetching meta file from backend if in restoring mode */
	if (hcfs_system->system_restoring == RESTORING_STAGE2) {
		ret = restore_meta_super_block_entry(new_info->f_ino, NULL);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}
	}

	ret = fetch_meta_path(new_info->rootpath, new_info->f_ino);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}
	strcpy((new_info->f_name), fsname);
	new_info->f_mp = malloc((sizeof(char) * strlen(mp)) + 10);
	if (new_info->f_mp == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of memory in %s\n", __func__);
		goto errcode_handle;
	}
	strcpy((new_info->f_mp), mp);

	new_info->lookup_table = malloc(sizeof(LOOKUP_HEAD_TYPE)
				* NUM_LOOKUP_ENTRIES);
	if (new_info->lookup_table == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of memory in %s\n", __func__);
		goto errcode_handle;
	}
	ret = lookup_init(new_info->lookup_table);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	/* Init fs_stat */
	new_info->FS_stat = malloc(sizeof(FS_STAT_T));
	if (new_info->FS_stat == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of memory in %s\n", __func__);
		goto errcode_handle;
	}
	memset(new_info->FS_stat, 0, sizeof(FS_STAT_T));
	
	/* init sem of stat lock */
	new_info->stat_lock = malloc(sizeof(sem_t));
	if (new_info->stat_lock == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of memory in %s\n", __func__);
		goto errcode_handle;
	}
	sem_init(new_info->stat_lock, 0, 1);

	/* Open stat_fptr */
	ret = fetch_stat_path(temppath, new_info->f_ino);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	new_info->stat_fptr = fopen(temppath, "r+");
	if (new_info->stat_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error %d (%s)\n", errcode,
		          strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	/* read stat */
	ret = read_FS_statistics(new_info);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}
	ret = do_mount_FS(mp, new_info);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}
	ret = insert_mount(fsname, new_info);
	if (ret < 0) {
		write_log(0, "Unexpected error in mounting.\n");
		write_log(0, "Please unmount manually\n");
		/* TODO: do we unmount the FS immediately? */
		errcode = ret;
		goto errcode_handle;
	}

	if (strncmp(mp, "/data/app", strlen("data/app")) == 0)
		hcfs_system->data_app_root = new_info->f_ino;

	if (strncmp(mp, "/data/data", strlen("data/data")) == 0) {
		DIR_ENTRY dentry;

		/* Record root inode and check if mgmt app folder is created. */
		data_data_root = new_info->f_ino;
		write_log(10, "Debug mount: root of /data/data is %" PRIu64
		          "\n", (uint64_t) data_data_root);
		ret = lookup_dir(data_data_root, "com.hopebaytech.hcfsmgmt",
					&dentry, FALSE);
		if (ret == 0) {
			mgmt_app_is_created = TRUE;
			write_log(4, "mgmt pkg folder existed");
		} else {
			mgmt_app_is_created = FALSE;
			if (ret != -ENOENT)
				write_log(0, "Error: Cannot access mgmt"
					" pkg folder. Code %d", -ret);
		}
	}
	if (strncmp(mp, "/data/smartcache", strlen("data/smartcache")) == 0) {
		data_smart_root = new_info->f_ino;
		write_log(10, "Debug mount: root of /data/smartcache is %" PRIu64
		          "\n", (uint64_t) data_smart_root);
	}
	sem_post(&(mount_mgr.mount_lock));
	sem_post(&(fs_mgr_head->op_lock));

	return 0;

errcode_handle:
	if (new_info != NULL) {
		free(new_info->f_mp);
		free(new_info->lookup_table);
		if (new_info->stat_fptr != NULL)
			fclose(new_info->stat_fptr);
		free(new_info);
	}
	sem_post(&(mount_mgr.mount_lock));
	sem_post(&(fs_mgr_head->op_lock));
	return errcode;
}

#ifdef _ANDROID_ENV_
static int32_t _check_destroy_vol_shared_data(MOUNT_T *mount_info)
{
	int32_t ret;
	MOUNT_T *tmp_info;

	/* Search any mountpoint of given volume name */
	ret = search_mount(mount_info->f_name, NULL, &tmp_info);
	if (ret < 0) {
		/* Destroy shared data when all mountpoints are unmounted */
		if (ret == -ENOENT) {
			write_log(8, "Destroy shared data of volume %s\n",
					mount_info->f_name);
			lookup_destroy(mount_info->lookup_table, mount_info);
			free(mount_info->lookup_table);
			mount_info->lookup_table = NULL;
			if (mount_info->stat_fptr != NULL)
				fclose(mount_info->stat_fptr);
			if (mount_info->FS_stat != NULL)
				free(mount_info->FS_stat);
			if (mount_info->stat_lock != NULL) {
				sem_destroy(mount_info->stat_lock);
				free(mount_info->stat_lock);
			}
			if (mount_info->vol_path_cache != NULL) {
				ret = destroy_pathcache(
						mount_info->vol_path_cache);
				if (ret < 0) {
					write_log(0, "Error: Fail to destroy"
						"path cache in %s. Code %d\n",
						__func__, -ret);
					return ret;
				}
			}
		} else {
			write_log(0, "Error: Search mount fail in %s.Code %d\n",
					__func__, -ret);
			return ret;
		}
	}

	return 0;
}
#endif

/************************************************************************
*
* Function name: unmount_FS
*        Inputs: char *fsname
*       Summary: Unmount filesystem "fsname".
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t unmount_FS(char *fsname, char *mp)
{
	int32_t ret, errcode;
	MOUNT_T *ret_info = NULL;
	MOUNT_NODE_T *ret_node = NULL;

	sem_wait(&(fs_mgr_head->op_lock));
	sem_wait(&(mount_mgr.mount_lock));

	/* Need to unmount FUSE and set is_unmount */

	ret = search_mount(fsname, mp, &ret_info);
	if (ret < 0) {
		if (ret == -ENOENT)
			write_log(2, "Unmount error: Not mounted\n");
		else
			write_log(0, "Unexpected error in unmounting.\n");
		errcode = ret;
		goto errcode_handle;
	}

	ret_info->is_unmount = TRUE;
	fuse_set_signal_handlers(ret_info->session_ptr);
	pthread_kill(ret_info->mt_thread, SIGHUP);

	do_unmount_FS(ret_info);

	/* TODO: Error handling for failed unmount such as block mp */
	ret = delete_mount(fsname, ret_info->f_mp, &ret_node);
#ifdef _ANDROID_ENV_
	_check_destroy_vol_shared_data(ret_info);
#endif

	if (ret_node != NULL) {
		if (ret_node->mt_entry != NULL) {
			free((ret_node->mt_entry)->f_mp);
			free(ret_node->mt_entry);
		}
		free(ret_node);
	}

	sem_post(&(mount_mgr.mount_lock));
	sem_post(&(fs_mgr_head->op_lock));

	return ret;

errcode_handle:
	sem_post(&(mount_mgr.mount_lock));
	sem_post(&(fs_mgr_head->op_lock));
	return errcode;
}

/************************************************************************
*
* Function name: unmount_event
*        Inputs: char *fsname
*       Summary: Unmount filesystem "fsname" when exiting FUSE loop.
*
*  Return value: None
*
*          Note: If is_unmount is set, should not call this when exiting
*                FUSE loop
*************************************************************************/
int32_t unmount_event(char *fsname, char *mp)
{
	int32_t ret, errcode;
	MOUNT_T *ret_info = NULL;
	MOUNT_NODE_T *ret_node = NULL;

	write_log(10, "Unmounting FS %s\n", fsname);

	sem_wait(&(fs_mgr_head->op_lock));
	sem_wait(&(mount_mgr.mount_lock));

	ret = search_mount(fsname, mp, &ret_info);
	if (ret < 0) {
		if (ret == -ENOENT)
			write_log(2, "Unmount error: Not mounted\n");
		else
			write_log(0, "Unexpected error in unmounting.\n");
		errcode = ret;
		goto errcode_handle;
	}

	do_unmount_FS(ret_info);

	ret = delete_mount(fsname, mp, &ret_node);
#ifdef _ANDROID_ENV_
	_check_destroy_vol_shared_data(ret_info);
#endif

	if (ret_node != NULL) {
		if (ret_node->mt_entry != NULL) {
			free((ret_node->mt_entry)->f_mp);
			free(ret_node->mt_entry);
		}
		free(ret_node);
	}

	sem_post(&(mount_mgr.mount_lock));
	sem_post(&(fs_mgr_head->op_lock));

	return ret;

errcode_handle:
	sem_post(&(mount_mgr.mount_lock));
	sem_post(&(fs_mgr_head->op_lock));
	return errcode;
}

/************************************************************************
*
* Function name: mount_status
*        Inputs: char *fsname
*       Summary: Checks if "fsname" is mounted.
*
*  Return value: 0 if found. -ENOENT if not found.
*                On error, returns negation of error code.
*************************************************************************/
int32_t mount_status(char *fsname)
{
	int32_t ret;

	sem_wait(&(fs_mgr_head->op_lock));
	sem_wait(&(mount_mgr.mount_lock));
	ret = FS_is_mounted(fsname);
	sem_post(&(mount_mgr.mount_lock));
	sem_post(&(fs_mgr_head->op_lock));

	return ret;
}

/************************************************************************
*
* Function name: unmount_all
*        Inputs: None
*       Summary: Unmount all filesystems.
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t unmount_all(void)
{
	/* TODO: errcode_handle */
	MOUNT_T *ret_info = NULL;
	MOUNT_NODE_T *ret_node = NULL;
	char fsname[MAX_FILENAME_LEN+1];
	int32_t ret = 0;

	sem_wait(&(fs_mgr_head->op_lock));
	sem_wait(&(mount_mgr.mount_lock));

	/* Need to unmount FUSE and set is_unmount */
	while (mount_mgr.root != NULL) {
		/* If exists some FS */
		strcpy(fsname, ((mount_mgr.root)->mt_entry)->f_name);
		ret_info = (mount_mgr.root)->mt_entry;

		ret_info->is_unmount = TRUE;
		fuse_set_signal_handlers(ret_info->session_ptr);
		pthread_kill(ret_info->mt_thread, SIGHUP);
		do_unmount_FS(ret_info);

		/* TODO: check return value */
		write_log(5, "Unmounted filesystem %s at mountpoint %s\n",
				fsname, ret_info->f_mp);
		ret = delete_mount(fsname, ret_info->f_mp, &ret_node);
#ifdef _ANDROID_ENV_
		_check_destroy_vol_shared_data(ret_info);
#endif

		if (ret_node != NULL) {
			if (ret_node->mt_entry != NULL) {
				free((ret_node->mt_entry)->f_mp);
				free(ret_node->mt_entry);
			}
			free(ret_node);
			ret_node = NULL;
		}
		if (ret < 0)
			break;
	}

	sem_post(&(mount_mgr.mount_lock));
	sem_post(&(fs_mgr_head->op_lock));

	return ret;
}

void _write_volstat(void *mptr1)
{
	MOUNT_T *mptr;

	mptr = (MOUNT_T *) mptr1;

	sleep(2);
	sem_wait((mptr->stat_lock));
	update_FS_statistics(mptr);
	mptr->writing_stat = FALSE;
	write_log(10, "Writing vol statistics\n");
	sem_post((mptr->stat_lock));
	return;
}
/************************************************************************
*
* Function name: change_mount_stat
*        Inputs: MOUNT_T *mptr, int64_t system_size_delta,
*                int64_t num_inodes_delta
*       Summary: Update the per-FS statistics in the mount table
*                for the mount pointed by "mptr". Also sync the content
*                to the xattr of the root meta file.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t change_mount_stat(MOUNT_T *mptr, int64_t system_size_delta,
		int64_t meta_size_delta, int64_t num_inodes_delta)
{
	//int32_t ret;
	pthread_t write_volstat_thread;

	sem_wait((mptr->stat_lock));
	(mptr->FS_stat)->system_size += (system_size_delta + meta_size_delta);
	if ((mptr->FS_stat)->system_size < 0)
		(mptr->FS_stat)->system_size = 0;

	(mptr->FS_stat)->meta_size += meta_size_delta;
	if ((mptr->FS_stat)->meta_size < 0)
		(mptr->FS_stat)->meta_size = 0;

	(mptr->FS_stat)->num_inodes += num_inodes_delta;
	if ((mptr->FS_stat)->num_inodes < 0)
		(mptr->FS_stat)->num_inodes = 0;
	if (mptr->writing_stat == FALSE) {
		mptr->writing_stat = TRUE;
		pthread_create(&(write_volstat_thread),
			&prefetch_thread_attr, (void *)&_write_volstat,
			(void *)mptr);
	}

	//ret = update_FS_statistics(mptr);
	sem_post((mptr->stat_lock));

	return 0;
}

/************************************************************************
*
* Function name: update_FS_statistics
*        Inputs: MOUNT_T *mptr
*       Summary: Update per-FS statistics to the stat file of root inode.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t update_FS_statistics(MOUNT_T *mptr)
{
	int32_t errcode;
	int32_t tmpfd;
	ssize_t ret_ssize;

	tmpfd = fileno(mptr->stat_fptr);

	if (tmpfd > 0)
		PWRITE(tmpfd, (mptr->FS_stat), sizeof(FS_STAT_T), 0);
	else
		write_log(4, "Warn: Bad fd in %s. Skip writing.", __func__);

	/* Remove fsync for the purpose of write performance */
	//FSYNC(tmpfd);

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: read_FS_statistics
*        Inputs: MOUNT_T *mptr
*       Summary: Read per-FS statistics from the stat file of root inode.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t read_FS_statistics(MOUNT_T *mptr)
{
	int32_t errcode;
	int32_t tmpfd;
	ssize_t ret_ssize;

	tmpfd = fileno(mptr->stat_fptr);

	PREAD(tmpfd, (mptr->FS_stat), sizeof(FS_STAT_T), 0);

	return 0;

errcode_handle:
	return errcode;
}
