/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: FS_manager.c
* Abstract: The C source file for mount manager
*
* Revision History
* 2015/7/7 Jiahong created this file
* 2015/7/15 Jiahong adding content
*
**************************************************************************/

#include "mount_manager.h"

#include <errno.h>
#include <string.h>

#include "global.h"
#include "FS_manager.h"

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
int search_mount(char *fsname, MOUNT_T **mt_info)
{
	int ret, errcode;	
	MOUNT_NODE_T *root;

	root = mount_mgr.root;
	if (root == NULL) {
		errcode = -ENOENT;
		goto errcode_handle;
	}

	ret = search_mount_node(fsname, mount_mgr.root, mt_info);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}


	return 0;

errcode_handle:
	return errcode;
}

/* Helper function for searching binary tree */
int search_mount_node(char *fsname, MOUNT_NODE_T *node, MOUNT_T **mt_info)
{
	int ret;
	MOUNT_NODE_T *next;

	ret = strcmp(fsname, (node->mt_entry)->f_name);
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
		return search_mount_node(fsname, next, mt_info);
	}

	/* Search the left sub-tree */
	next = node->lchild;
	if (next == NULL)
		return -ENOENT;
	return search_mount_node(fsname, next, mt_info);
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
int insert_mount(char *fsname, MOUNT_T *mt_info)
{
	int ret, errcode;	
	MOUNT_NODE_T *root;

	root = mount_mgr.root;
	if (root == NULL) {
		/* Add to root */
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
int insert_mount_node(char *fsname, MOUNT_NODE_T *node, MOUNT_T *mt_info)
{
	int ret;
	MOUNT_NODE_T *next;

	ret = strcmp(fsname, (node->mt_entry).f_name);
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
		(node->rchild)->parent = node;
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
*        Inputs: char *fsname, MOUNT_NODE_T *node
*       Summary: Delete "fsname" in the mount database. If deleted, the node
*                is not freed immediately, but will be returned via "node".
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int delete_mount(char *fsname, MOUNT_NODE_T *ret_node)
{
	int ret, errcode;	
	MOUNT_NODE_T *root;

	root = mount_mgr.root;
	if (root == NULL) {
		/* Nothing to delete */
		errcode = -ENOENT;
		goto errcode_handle;
	}

	ret = delete_mount_node(fsname, mount_mgr.root, ret_node);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	return 0;

errcode_handle:
	return errcode;
}

void replace_with_largest_child(MOUNT_NODE_T *node, MOUNT_NODE_T *ret_node)
{
	if (node->rchild == NULL) {

		ret_node = node;
		/* If no children, update parent and return */
		if (node->lchild == NULL) {
			if (((node->parent)->lchild) == node)
				(node->parent)->lchild = NULL;
			else
				(node->parent)->rchild = NULL;
			return;
		}

		/* Replace self with left subtree */
		if (((node->parent)->lchild) == node)
			(node->parent)->lchild = node->lchild;
		else
			(node->parent)->rchild = node->lchild;
		return;
	}

	return replace_with_largest_child(node->rchild, ret_node);
}

int delete_mount_node(char *fsname, MOUNT_NODE_T *node, MOUNT_NODE_T *ret_node)
{
	int ret;
	MOUNT_NODE_T *next;
	MOUNT_NODE_T *ret_child;

	ret = strcmp(fsname, (node->mt_entry)->f_name);
	/* Check if this is the one */
	if (ret == 0) {
		mount_mgr.num_mt_FS--;
		ret_node = node;
		/* If no children, update parent and return */
		if ((node->lchild == NULL) && (node->rchild == NULL)) {
			if (node->parent == NULL) {
				/* This is the root */
				mount_mgr.root = NULL;
				return 0;
			}
			if (((node->parent)->lchild) == node)
				(node->parent)->lchild = NULL;
			else
				(node->parent)->rchild = NULL;
			return 0;
		}

		if (node->lchild == NULL) {
			/* Replace self with right subtree */
			if (node->parent == NULL) {
				/* This is the root */
				mount_mgr.root = node->rchild;
				return 0;
			}
			if (((node->parent)->lchild) == node)
				(node->parent)->lchild = node->rchild;
			else
				(node->parent)->rchild = node->rchild;
			return 0;
		}

		if (node->rchild == NULL) {
			/* Replace self with left subtree */
			if (node->parent == NULL) {
				/* This is the root */
				mount_mgr.root = node->lchild;
				return 0;
			}
			if (((node->parent)->lchild) == node)
				(node->parent)->lchild = node->lchild;
			else
				(node->parent)->rchild = node->lchild;
			return 0;
		}


		/* Delete this, but need to find the largest child to
			replace */
		replace_with_largest_child(node->lchild, ret_child);
		if (node->parent == NULL) {
			/* This is the root */
			mount_mgr.root = ret_child;
			return 0;
		}
		if (((node->parent)->lchild) == node)
			(node->parent)->lchild = ret_child;
		else
			(node->parent)->rchild = ret_child;

		return 0;
	}

	if (ret > 0) {
		/* Search the right sub-tree */
		next = node->rchild;
		if (next == NULL)
			return -ENOENT;
		return delete_mount_node(fsname, next, ret_node);
	}

	/* Search the left sub-tree */
	next = node->lchild;
	if (next == NULL)
		return -ENOENT;
	return delete_mount_node(fsname, next, ret_node);
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
int init_mount_mgr(void)
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
int destroy_mount_mgr(void)
{
	int ret;

	ret = unmount_all();
	/* All mounted FS should be processed. */

	return ret;
}

int FS_is_mounted(char *fsname)
{
	int ret;
	MOUNT_T tmpinfo;

	ret = search_mount(fsname, &tmpinfo);
	if (ret == 0)
		return TRUE;

	return FALSE;
}

/* Helper for mounting */
int do_mount_FS(char *mp, MOUNT_T *new_info)
{
	struct fuse_chan *tmp_channel;
	struct fuse_session *tmp_session;

	/* f_ino, f_name, f_mp are filled before calling this function */
	/* global_fuse_args is stored in fuseop.h */
	tmp_channel = fuse_mount(mp, &global_fuse_args);
	tmp_session = fuse_lowlevel_new(&global_fuse_args,
			&hfuse_ops, sizeof(hfuse_ops), NULL);
	fuse_set_signal_handlers(tmp_session);
	fuse_session_add_chan(tmp_session, tmp_channel);
	gettimeofday(&(new_info->mt_time), NULL);
	new_info->session_ptr = tmp_session;
	new_info->chan_ptr = tmp_channel;
	new_info->is_unmount = FALSE;

	pthread_create(&(new_info->mt_thread), NULL, fuse_session_loop_mt,
			(void *)new_info);
	/* TODO: checking for failed mount */
	return 0;
}
/* Helper for unmounting */
int do_unmount_FS(MOUNT_T *mount_info)
{
	pthread_join(mount_info->mt_thread);
	fuse_session_remove_chan(mount_info->chan_ptr);
	fuse_remove_signal_handlers(mount_info->session_ptr);
	fuse_session_destroy(mount_info->session_ptr);
	fuse_unmount(mount_info->f_mp, mount_info->chan_ptr);
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
int mount_FS(char *fsname, char *mp) {

	int ret, errcode;
	MOUNT_T tmp_info, *new_info;
	DIR_ENTRY tmp_entry;

	sem_wait(&(fs_mgr_head->op_lock));
	sem_wait(&(mount_mgr.mount_lock));

	/* TODO: check if mp is a valid mountpoint */

	/* First check if FS already mounted */
	new_info = NULL;

	ret = search_mount(fsname, &tmp_info);
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

	new_info->f_ino = tmp_entry.d_ino;
	strcpy(&(new_info->f_name), fsname);
	new_info->f_mp = malloc((sizeof(char) * strlen(mp)) + 10);
	if (new_info->f_mp == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of memory in %s\n", __func__);
		goto errcode_handle;
	}
	strcpy(&(new_info->f_mp), mp);

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

	sem_post(&(fs_mgr_head->op_lock));
	sem_post(&(mount_mgr.mount_lock));

	return 0;

errcode_handle:
	sem_post(&(fs_mgr_head->op_lock));
	sem_post(&(mount_mgr.mount_lock));
	return errcode;
}

/************************************************************************
*
* Function name: unmount_FS
*        Inputs: char *fsname
*       Summary: Unmount filesystem "fsname".
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int unmount_FS(char *fsname)
{
	int ret, errcode;
	MOUNT_T *ret_info;
	MOUNT_NODE_T *ret_node;

	sem_wait(&(fs_mgr_head->op_lock));
	sem_wait(&(mount_mgr.mount_lock));

	/* Need to unmount FUSE and set is_unmount */

	ret = search_mount(fsname, &ret_info);
	if (ret < 0) {
		if (ret == -ENOENT)
			write_log(2, "Unmount error: Not mounted\n");
		else
			write_log(0, "Unexpected error in unmounting.\n");
		errcode = ret;
		goto errcode_handle;
	}


	ret_info->is_unmount = TRUE;
	pthread_kill(ret_info->mt_thread, SIGHUP);
	do_unmount_FS(ret_info);

	ret = delete_mount(fsname, &ret_node);

	free((ret_node->mt_entry)->f_mp);
	free(ret_node->mt_entry);
	free(ret_node);

	sem_post(&(fs_mgr_head->op_lock));
	sem_post(&(mount_mgr.mount_lock));

	return 0;

errcode_handle:
	sem_post(&(fs_mgr_head->op_lock));
	sem_post(&(mount_mgr.mount_lock));
	return errcode;
}
int unmount_all(void);

/* If is_unmount is set, FUSE destroy routine should not call
unmount_event */
int unmount_event(char *fsname); /* Called by FUSE destroy */
int mount_status(char *fsname);


