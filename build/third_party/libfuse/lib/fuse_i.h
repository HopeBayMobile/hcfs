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

#include "fuse/fuse.h"
#include "fuse/fuse_lowlevel.h"
#include <pthread.h>

struct fuse_chan;
struct fuse_ll;

struct fuse_session {
	struct fuse_session_ops op;

	int (*receive_buf)(struct fuse_session *se, struct fuse_buf *buf,
			   struct fuse_chan **chp);

	void (*process_buf)(void *data, const struct fuse_buf *buf,
			    struct fuse_chan *ch);

	void *data;

	volatile int exited;

	struct fuse_chan *ch;
};

struct fuse_req {
	struct fuse_ll *f;
	uint64_t unique;
	int ctr;
	pthread_mutex_t lock;
	struct fuse_ctx ctx;
	struct fuse_chan *ch;
	int interrupted;
	unsigned int ioctl_64bit : 1;
	union {
		struct {
			uint64_t unique;
		} i;
		struct {
			fuse_interrupt_func_t func;
			void *data;
		} ni;
	} u;
	struct fuse_req *next;
	struct fuse_req *prev;
};

struct fuse_notify_req {
	uint64_t unique;
	void (*reply)(struct fuse_notify_req *, fuse_req_t, fuse_ino_t,
		      const void *, const struct fuse_buf *);
	struct fuse_notify_req *next;
	struct fuse_notify_req *prev;
};

struct fuse_ll {
	int debug;
	int allow_root;
	int atomic_o_trunc;
	int no_remote_posix_lock;
	int no_remote_flock;
	int big_writes;
	int splice_write;
	int splice_move;
	int splice_read;
	int no_splice_write;
	int no_splice_move;
	int no_splice_read;
	int writeback_cache;
	struct fuse_lowlevel_ops op;
	int got_init;
	struct cuse_data *cuse_data;
	void *userdata;
	uid_t owner;
	struct fuse_conn_info conn;
	struct fuse_req list;
	struct fuse_req interrupts;
	pthread_mutex_t lock;
	int got_destroy;
	pthread_key_t pipe_key;
	int broken_splice_nonblock;
	uint64_t notify_ctr;
	struct fuse_notify_req notify_list;
};

struct fuse_cmd {
	char *buf;
	size_t buflen;
	struct fuse_chan *ch;
};

struct fuse *fuse_new_common(struct fuse_chan *ch, struct fuse_args *args,
			     const struct fuse_operations *op,
			     size_t op_size, void *user_data, int compat);

int fuse_sync_compat_args(struct fuse_args *args);

struct fuse_chan *fuse_kern_chan_new(int fd);

struct fuse_session *fuse_lowlevel_new_common(struct fuse_args *args,
					const struct fuse_lowlevel_ops *op,
					size_t op_size, void *userdata);

void fuse_kern_unmount_compat22(const char *mountpoint);
int fuse_chan_clearfd(struct fuse_chan *ch);

void fuse_kern_unmount(const char *mountpoint, int fd);
/* Jiahong (1/15/2016) modified fuse_kern_mount to add a premount routine */
int fuse_kern_premount(const char *mountpoint, struct fuse_args *args);
int fuse_kern_mount(const char *mountpoint, struct fuse_args *args, int fd);

int fuse_send_reply_iov_nofree(fuse_req_t req, int error, struct iovec *iov,
			       int count);
void fuse_free_req(fuse_req_t req);


struct fuse *fuse_setup_common(int argc, char *argv[],
			       const struct fuse_operations *op,
			       size_t op_size,
			       char **mountpoint,
			       int *multithreaded,
			       int *fd,
			       void *user_data,
			       int compat);

void cuse_lowlevel_init(fuse_req_t req, fuse_ino_t nodeide, const void *inarg);

int fuse_start_thread(pthread_t *thread_id, void *(*func)(void *), void *arg);
