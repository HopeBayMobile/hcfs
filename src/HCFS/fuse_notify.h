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

#include "macro.h"
#include "mount_manager.h"
#include <semaphore.h>

#define FUSE_NOTIFY_CB_DEFAULT_LEN 8
#define FUSE_NOTIFY_CB_ELEMSIZE 52

enum NOTIFY_FUNCTION { NOOP, DELETE };
enum NOTIFY_ACTION { RUN, DESTROY_CB };

#define FUSE_NOTIFY_PROTO_MEMBER enum NOTIFY_FUNCTION func;

/* Cycle buffer and it's data slots */
typedef struct {
	uint8_t padding[FUSE_NOTIFY_CB_ELEMSIZE];
} _PACKED FUSE_NOTIFY_DATA;
typedef struct {
	FUSE_NOTIFY_DATA *elems;
	size_t max_len;
	size_t len;
	size_t in;
	size_t out;
	sem_t tasks_sem;
	sem_t access_sem;
	BOOL is_initialized;
} FUSE_NOTIFY_CYCLE_BUF;

/* Actual notify data defenition */
typedef struct {
	FUSE_NOTIFY_PROTO_MEMBER
} _PACKED FUSE_NOTIFY_PROTO;

typedef struct {
	FUSE_NOTIFY_PROTO_MEMBER
	struct fuse_chan *ch;
	fuse_ino_t parent;
	fuse_ino_t child;
	char *name;
	size_t namelen;
} _PACKED FUSE_NOTIFY_DELETE_DATA;

typedef void(fuse_notify_fn)(FUSE_NOTIFY_DATA **, enum NOTIFY_ACTION);
fuse_notify_fn _do_hfuse_ll_notify_noop;
fuse_notify_fn _do_hfuse_ll_notify_delete;

int32_t init_notify_cb(void);
void destory_notify_cb(void);
BOOL notify_cb_realloc(void);
void notify_cb_enqueue(const void *const notify);
FUSE_NOTIFY_DATA *notify_cb_dequeue();

int32_t init_hfuse_ll_notify_loop(void);
void destory_hfuse_ll_notify_loop(void);
void *hfuse_ll_notify_loop(void *ptr);
void hfuse_ll_notify_delete(struct fuse_chan *ch,
			    fuse_ino_t parent,
			    fuse_ino_t child,
			    const char *name,
			    size_t namelen);
void hfuse_ll_notify_delete_mp(struct fuse_chan *ch,
			       fuse_ino_t parent,
			       fuse_ino_t child,
			       const char *name,
			       size_t namelen,
			       const char *selfname);
#endif /* SRC_HCFS_FUSE_NOTIFY_H_ */
