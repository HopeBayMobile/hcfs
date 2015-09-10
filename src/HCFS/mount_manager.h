/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: FS_manager.h
* Abstract: The header file for mount manager
*
* Revision History
* 2015/7/7 Jiahong created this file
* 2015/7/15 Jiahong adding most content
* 2015/7/31 Jiahong adding data structure for FS statistics
*
**************************************************************************/

#ifndef GW20_HCFS_MOUNT_MANAGER_H_
#define GW20_HCFS_MOUNT_MANAGER_H_

#include <sys/time.h>
#include <pthread.h>
#include <fuse/fuse_lowlevel.h>
#include <semaphore.h>

#include "params.h"
#include "fuseop.h"
#include "lookup_count_types.h"

/*
Binary search tree

Node content
Name of the mounted filesystem and root inode number
Mountpoint of the filesystem
Time of the mount operation on this filesystem
ID of the thread forked for calling FUSE loop
Pointers to the FUSE session and channel used for this mount
*/

typedef struct {
	long long system_size;
	long long num_inodes;
} FS_STAT_T;

typedef struct {
	ino_t f_ino;
	char f_name[MAX_FILENAME_LEN+1];
	char rootpath[METAPATHLEN];
	char *f_mp;
	struct timeval mt_time;
	pthread_t mt_thread;
	struct fuse_session *session_ptr;
	struct fuse_chan *chan_ptr;
	char is_unmount;
	LOOKUP_HEAD_TYPE *lookup_table;
	FS_STAT_T FS_stat;
	FILE *stat_fptr;  /* For keeping track of FS stat */
	sem_t stat_lock;
	struct fuse_args mount_args;
} MOUNT_T;

struct mount_node_template {
	MOUNT_T *mt_entry;
	struct mount_node_template *lchild;
	struct mount_node_template *rchild;
	struct mount_node_template *parent;
};

typedef struct mount_node_template MOUNT_NODE_T;

typedef struct {
	long num_mt_FS;
	MOUNT_NODE_T *root;
	sem_t mount_lock;
} MOUNT_MGR_T;

MOUNT_MGR_T mount_mgr;

/* Routines should also lock FS manager if needed to prevent inconsistency
in the two modules */

int init_mount_mgr(void);
int destroy_mount_mgr(void); /* Will call unmount_all */

int mount_FS(char *fsname, char *mp);
int unmount_FS(char *fsname);  /* Need to unmount FUSE and set is_unmount */
int unmount_all(void);

/* If is_unmount is set, FUSE destroy routine should not call
unmount_event */
int unmount_event(char *fsname);
int mount_status(char *fsname);

/* Below are helper functions. Will not process lock / unlock in these
functions */
int FS_is_mounted(char *fsname);

int search_mount_node(char *fsname, MOUNT_NODE_T *node, MOUNT_T **mt_info);
int search_mount(char *fsname, MOUNT_T **mt_info);
int insert_mount_node(char *fsname, MOUNT_NODE_T *node, MOUNT_T *mt_info);
int insert_mount(char *fsname, MOUNT_T *mt_info);

/* For delete from tree, the tree node will not be freed immediately. This
should be handled in the unmount routines */
int delete_mount_node(char *fsname, MOUNT_NODE_T *node,
					MOUNT_NODE_T **ret_node);
int delete_mount(char *fsname, MOUNT_NODE_T **ret_node);

int change_mount_stat(MOUNT_T *mptr, long long system_size_delta,
				long long num_inodes_delta);
int update_FS_statistics(MOUNT_T *mptr);

int read_FS_statistics(MOUNT_T *mptr);
#endif  /* GW20_HCFS_MOUNT_MANAGER_H_ */

