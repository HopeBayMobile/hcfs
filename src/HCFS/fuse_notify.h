#ifndef SRC_HCFS_FUSE_NOTIFY_H
#define SRC_HCFS_FUSE_NOTIFY_H

#include <semaphore.h>
#include "macro.h"
#include "mount_manager.h"

#define FUSE_NOTIFY_CB_DEFAULT_LEN 2

/* Use FUSE_NOTIFY to determin notify type before load args */
typedef struct {
	void (*notify_func)(void *);
	size_t data_size;
} _PACKED FUSE_NOTIFY_PROTO;

typedef struct {
	void (*func)(void *);
	size_t data_size;
	struct fuse_chan *ch;
	fuse_ino_t parent;
	fuse_ino_t child;
	const char *name;
	size_t namelen;
} _PACKED FUSE_NOTIFY_DELETE_DATA;

typedef struct {
	void *elems;
	size_t max_len;
	size_t len;
	size_t in;
	size_t out;
	sem_t tasks_sem;
	size_t elemsize;
} FUSE_NOTIFY_CYCLE_BUF;

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
#endif  /* SRC_HCFS_FUSE_NOTIFY_H */
