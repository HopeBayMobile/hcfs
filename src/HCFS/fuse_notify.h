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

#define FUSE_NOTIFY_CB_DEFAULT_LEN 2

enum NOTIFY_ACTION { RUN, DESTROY_CB };
/* Use FUSE_NOTIFY to determin notify type before load args */
typedef struct _notify_proto {
	/* notify prototype */
	void (*func)(struct _notify_proto **, enum NOTIFY_ACTION);
	size_t data_size;
} _PACKED FUSE_NOTIFY_PROTO;

typedef struct {
	FUSE_NOTIFY_PROTO proto;
	struct fuse_chan *ch;
	fuse_ino_t parent;
	fuse_ino_t child;
	char *name;
	size_t namelen;
} _PACKED FUSE_NOTIFY_DELETE_DATA;

void _do_hfuse_ll_notify_delete(FUSE_NOTIFY_PROTO **data, enum NOTIFY_ACTION);

typedef struct {
	void *elems;
	size_t max_len;
	size_t len;
	size_t in;
	size_t out;
	size_t elemsize;
	sem_t tasks_sem;
	sem_t access_sem;
} FUSE_NOTIFY_CYCLE_BUF;

void init_hfuse_ll_notify_loop(void);
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
