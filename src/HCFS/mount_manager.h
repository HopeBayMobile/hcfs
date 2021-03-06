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
#include "pthread_control.h"

enum MP_TYPE {
	MP_DEFAULT = 1,
	MP_READ,
	MP_WRITE,
	__MP_TYPE_END /* Set total mp type number. Keep this at end of list */
};
#define MP_TYPE_NUM (__MP_TYPE_END - MP_DEFAULT)

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
	int64_t disk_pinned_size;
	int64_t disk_meta_size;
	char fileID[GDRIVE_ID_LENGTH + 1]; /* For backend googledrive */
} FS_CLOUD_STAT_T_V2;

typedef struct {
	int64_t backend_system_size;
	int64_t backend_meta_size;
	int64_t backend_num_inodes;
	int64_t max_inode;
	int64_t pinned_size;
} FS_CLOUD_STAT_T_V1;

#define FS_CLOUD_STAT_T FS_CLOUD_STAT_T_V2

/* TODO: If we have multiple mount volumns with mp mode, MOUNT_T_GLOBAL need be
 * privatized and shared between views of same volumn */
enum MP_CHAN_TYPE { FUSE_DEFAULT, FUSE_WRITE, FUSE_READ };
typedef struct {
	struct fuse_chan *ch[MP_TYPE_NUM + 1];
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
	BOOL is_unmount;
	LOOKUP_HEAD_TYPE *lookup_table; /* All vol share the same one */
	FS_STAT_T *FS_stat; /* shared */
	FILE *stat_fptr;  /* For keeping track of FS stat. shared */
	sem_t *stat_lock; /* shared */
	BOOL writing_stat;
	PTHREAD_REUSE_T *write_volstat_thread;
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

