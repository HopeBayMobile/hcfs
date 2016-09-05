/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: FS_manager.h
* Abstract: The header file for mount manager
*
* Revision History
* 2015/7/7 Jiahong created this file
* 2015/7/15 Jiahong adding most content
* 2015/7/31 Jiahong adding data structure for FS statistics
* 2016/4/13 Jiahong reorganizing pkg lookup cache to global
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
#ifdef _ANDROID_ENV_
#include "path_reconstruct.h"
#endif

#define MP_DEFAULT 1
#define MP_READ 2
#define MP_WRITE 3

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
	int64_t system_size;
	int64_t meta_size;
	int64_t num_inodes;
} FS_STAT_T;

typedef struct {
	int64_t backend_system_size;
	int64_t backend_meta_size;
	int64_t backend_num_inodes;
	int64_t max_inode;
	int64_t pinned_size;
} FS_CLOUD_STAT_T;

/* TODO: If we have multiple mount volumns with mp mode, MOUNT_T_GLOBAL need be
 * privatized and shared between views of same volumn */
typedef struct {
	struct fuse_chan *fuse_default;
	struct fuse_chan *fuse_write;
	struct fuse_chan *fuse_read;
	sem_t sem;
} MOUNT_T_GLOBAL;

extern MOUNT_T_GLOBAL mount_global;

typedef struct {
	ino_t f_ino;
	char f_name[MAX_FILENAME_LEN+1];
#ifdef _ANDROID_ENV_
	char mp_mode;
	char volume_type;
	PATH_CACHE *vol_path_cache; /* Shared */
#endif
	char rootpath[METAPATHLEN];
	char *f_mp;
	struct timeval mt_time;
	pthread_t mt_thread;
	struct fuse_session *session_ptr;
	struct fuse_chan *chan_ptr;
	char is_unmount;
	LOOKUP_HEAD_TYPE *lookup_table; /* All vol share the same one */
	FS_STAT_T *FS_stat; /* shared */
	FILE *stat_fptr;  /* For keeping track of FS stat. shared */
	sem_t *stat_lock; /* shared */
	BOOL writing_stat;
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
	int64_t num_mt_FS;
	MOUNT_NODE_T *root;
	sem_t mount_lock;
} MOUNT_MGR_T;

MOUNT_MGR_T mount_mgr;

/* Routines should also lock FS manager if needed to prevent inconsistency
in the two modules */

int32_t init_mount_mgr(void);
int32_t destroy_mount_mgr(void); /* Will call unmount_all */

int32_t mount_FS(char *fsname, char *mp, char mp_mode);
int32_t unmount_FS(char *fsname, char *mp); /* Need to unmount FUSE and set is_unmount */
int32_t unmount_all(void);

/* If is_unmount is set, FUSE destroy routine should not call
unmount_event */
int32_t unmount_event(char *fsname, char *mp);
int32_t mount_status(char *fsname);

/* Below are helper functions. Will not process lock / unlock in these
functions */
int32_t FS_is_mounted(char *fsname);

int32_t search_mount_node(char *fsname, char *mp, MOUNT_NODE_T *node,
		MOUNT_T **mt_info);
int32_t search_mount(char *fsname, char *mp, MOUNT_T **mt_info);
int32_t insert_mount_node(char *fsname, MOUNT_NODE_T *node, MOUNT_T *mt_info);
int32_t insert_mount(char *fsname, MOUNT_T *mt_info);

/* For delete from tree, the tree node will not be freed immediately. This
should be handled in the unmount routines */
int32_t delete_mount_node(char *fsname, char *mp, MOUNT_NODE_T *node,
					MOUNT_NODE_T **ret_node);
int32_t delete_mount(char *fsname, char *mp, MOUNT_NODE_T **ret_node);

int32_t change_mount_stat(MOUNT_T *mptr, int64_t system_size_delta,
		int64_t meta_size_delta, int64_t num_inodes_delta);
int32_t update_FS_statistics(MOUNT_T *mptr);

int32_t read_FS_statistics(MOUNT_T *mptr);
#endif  /* GW20_HCFS_MOUNT_MANAGER_H_ */

