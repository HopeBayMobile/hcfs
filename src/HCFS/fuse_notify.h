/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: fuse_notify.h
* Abstract: The header file for FUSE notifications
*
* Revision History
* 2016/8/25 Jethro Add fuse_notify.h
* 2016/12/6 Jiahong Adding inval_entry notification
*
**************************************************************************/
#ifndef SRC_HCFS_FUSE_NOTIFY_H_
#define SRC_HCFS_FUSE_NOTIFY_H_

#include <semaphore.h>

#include "macro.h"
#include "mount_manager.h"

#define FUSE_NOTIFY_RINGBUF_MAXLEN 1024

typedef enum NOTIFY_FUNCTION { NOOP, DELETE, INVAL_ENT } NOTIFY_FUNCTION;
typedef enum NOTIFY_ACTION { RUN, DESTROY_BUF } NOTIFY_ACTION;

/* Actual notify data definitions */
typedef struct {
	NOTIFY_FUNCTION func;
	struct fuse_chan *ch;
	fuse_ino_t parent;
	fuse_ino_t child;
	char *name;
	size_t namelen;
} _PACKED FUSE_NOTIFY_DELETE_DATA;

typedef struct {
	NOTIFY_FUNCTION func;
	struct fuse_chan *ch;
	fuse_ino_t parent;
	char *name;
	size_t namelen;
} _PACKED FUSE_NOTIFY_INVAL_ENT_DATA;

/* Ring Buffer */

#define FUSE_NOTIFY_ENTRY_SIZE sizeof(FUSE_NOTIFY_DELETE_DATA)
/*
 * Prototype struct of FUSE_NOTIFY. It's size must equal to largest struct
 */
typedef struct {
	NOTIFY_FUNCTION func;
	uint8_t _[FUSE_NOTIFY_ENTRY_SIZE - sizeof(NOTIFY_FUNCTION)];
} _PACKED FUSE_NOTIFY_PROTO;

typedef struct FUSE_NOTIFY_LINKED_NODE {
	void *data;
	struct FUSE_NOTIFY_LINKED_NODE *next;
} FUSE_NOTIFY_LINKED_NODE;

typedef struct {
	FUSE_NOTIFY_PROTO ring_buf[FUSE_NOTIFY_RINGBUF_MAXLEN];
	FUSE_NOTIFY_LINKED_NODE *linked_list_head;
	FUSE_NOTIFY_LINKED_NODE *linked_list_rear;
	size_t len;
	size_t in;
	size_t out;
	sem_t not_empty;
	sem_t access_sem;
} FUES_NOTIFY_SHARED_DATA;

/* notify functions */
typedef int32_t(fuse_notify_fn)(FUSE_NOTIFY_PROTO *, enum NOTIFY_ACTION);
fuse_notify_fn _do_hfuse_ll_notify_noop;
fuse_notify_fn _do_hfuse_ll_notify_delete;
fuse_notify_fn _do_hfuse_ll_notify_inval_ent;

int32_t init_notify_buf(void);
void destory_notify_buf(void);
int32_t notify_buf_enqueue(const void *const notify);
FUSE_NOTIFY_PROTO *notify_buf_dequeue();

int32_t init_hfuse_ll_notify_loop(void);
int32_t destory_hfuse_ll_notify_loop(void);
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
int32_t hfuse_ll_notify_inval_ent(struct fuse_chan *ch,
			    fuse_ino_t parent,
			    const char *name,
			    size_t namelen);

#endif /* SRC_HCFS_FUSE_NOTIFY_H_ */
