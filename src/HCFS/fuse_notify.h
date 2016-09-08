/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: fuse_notify.h
* Abstract: The header file for FUSE notifications
*
* Revision History
* 2016/8/25 Jethro Add fuse_notify.h
*
**************************************************************************/
#ifndef SRC_HCFS_FUSE_NOTIFY_H_
#define SRC_HCFS_FUSE_NOTIFY_H_

#include <semaphore.h>

#include "macro.h"
#include "mount_manager.h"

#define FUSE_NOTIFY_BUF_MAX_LEN 1024
#define FUSE_NOTIFY_BUF_ELEMSIZE 52

enum NOTIFY_FUNCTION { NOOP, DELETE };
enum NOTIFY_ACTION { RUN, DESTROY_BUF };

/* Ring Buffer elements */

/* Data chunk */
typedef struct {
	uint8_t padding[FUSE_NOTIFY_BUF_ELEMSIZE];
} _PACKED FUSE_NOTIFY_DATA;

/* Common header among data chunks */
#define FUSE_NOTIFY_PROTO_MEMBER enum NOTIFY_FUNCTION func;
typedef struct {
	FUSE_NOTIFY_PROTO_MEMBER
} _PACKED FUSE_NOTIFY_PROTO;

/* Actual notify data defenitions */
typedef struct {
	FUSE_NOTIFY_PROTO_MEMBER
	struct fuse_chan *ch;
	fuse_ino_t parent;
	fuse_ino_t child;
	char *name;
	size_t namelen;
} _PACKED FUSE_NOTIFY_DELETE_DATA;

/* Ring Buffer */
typedef struct {
	FUSE_NOTIFY_DATA elems[FUSE_NOTIFY_BUF_MAX_LEN];
	size_t len;
	size_t in;
	size_t out;
	sem_t not_empty;
	sem_t not_full;
	sem_t access_sem;
} FUSE_NOTIFY_RING_BUF;

/* notify functions */
typedef int32_t(fuse_notify_fn)(FUSE_NOTIFY_DATA *, enum NOTIFY_ACTION);
fuse_notify_fn _do_hfuse_ll_notify_noop;
fuse_notify_fn _do_hfuse_ll_notify_delete;

int32_t init_notify_buf(void);
void destory_notify_buf(void);
int32_t notify_buf_enqueue(const void *const notify);
FUSE_NOTIFY_DATA *notify_buf_dequeue();

int32_t init_hfuse_ll_notify_loop(void);
void destory_hfuse_ll_notify_loop(void);
void *hfuse_ll_notify_loop(void *ptr);
int32_t hfuse_ll_notify_delete(struct fuse_chan *ch,
			    fuse_ino_t parent,
			    fuse_ino_t child,
			    const char *name,
			    size_t namelen);
int32_t hfuse_ll_notify_delete_mp(struct fuse_chan *ch,
			       fuse_ino_t parent,
			       fuse_ino_t child,
			       const char *name,
			       size_t namelen,
			       const char *selfname);

#endif /* SRC_HCFS_FUSE_NOTIFY_H_ */
