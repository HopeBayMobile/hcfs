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

/* Maximum data size of FUSE_NOTIFY, It should set to the largest structure. */
#define FUSE_NOTIFY_ENTRY_SIZE sizeof(FUSE_NOTIFY_DELETE_DATA)

/*
 * Prototype struct of FUSE_NOTIFY.
 */
typedef struct {
	NOTIFY_FUNCTION func;
	uint8_t _[FUSE_NOTIFY_ENTRY_SIZE - sizeof(NOTIFY_FUNCTION)];
} _PACKED FUSE_NOTIFY_PROTO;

typedef struct FUSE_NOTIFY_LINKED_NODE {
	FUSE_NOTIFY_PROTO *data;
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
int32_t notify_buf_enqueue(const FUSE_NOTIFY_PROTO *notify);
FUSE_NOTIFY_PROTO *notify_buf_dequeue(void);

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
