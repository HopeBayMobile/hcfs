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

#include "fuse/fuse_lowlevel.h"
#include "fuse_misc.h"
#include "fuse/fuse_kernel.h"
#include "fuse_i.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <errno.h>
#include <sys/time.h>
#include <stdint.h>

#ifdef __MULTI_THREAD

/* Environment var controlling the thread stack size */
#define ENVNAME_THREAD_STACK "FUSE_THREAD_STACK"

struct fuse_worker {
	struct fuse_worker *prev;
	struct fuse_worker *next;
	pthread_t thread_id;
	size_t bufsize;
	char *buf;
	struct fuse_mt *mt;
	int8_t cancelable;
	int8_t terminating;
};

static pthread_key_t sigkey;

struct fuse_mt {
	pthread_mutex_t lock;
	int numworker;
	int numavail;
	struct fuse_session *se;
	struct fuse_chan *prevch;
	struct fuse_worker main;
	sem_t finish;
	int exit;
	int error;
};

static void list_add_worker(struct fuse_worker *w, struct fuse_worker *next)
{
	struct fuse_worker *prev = next->prev;
	w->next = next;
	w->prev = prev;
	prev->next = w;
	next->prev = w;
}

static void list_del_worker(struct fuse_worker *w)
{
	struct fuse_worker *prev = w->prev;
	struct fuse_worker *next = w->next;
	prev->next = next;
	next->prev = prev;
}

#define PTHREAD_CANCEL_ENABLE 0
#define PTHREAD_CANCEL_DISABLE 1

static int fuse_loop_start_thread(struct fuse_mt *mt);

/* ADDED by seth
 * SIGUSR1 handler.
 * */
/* Jiahong (2016/10/18) using pthread_getspecific to obtain thread-specific
stat, and share SIGUSR1 */
void thread_exit_handler(int sig)
{
	struct fuse_worker *calling_ptr;

	calling_ptr = (struct fuse_worker *) pthread_getspecific(sigkey);

	if (calling_ptr == NULL)
		return;

	if (calling_ptr->cancelable != 0)
		pthread_exit(0);
	else
		calling_ptr->terminating = 1;
}

static void *fuse_do_work(void *data)
{
	struct fuse_worker *w = (struct fuse_worker *) data;
	struct fuse_mt *mt = w->mt;
	/* added by seth */
	/* Jiahong (10/13/2016) move sigaction to parent to avoid calling
	every time this thread is called */
	sigset_t sigset;

	/* Set action handler for SIGUSR1 */
	sighandler_init(&thread_exit_handler);

	pthread_setspecific(sigkey, data);

	/* Now use flags in fuse_worker to control whether
	the thread is cancelable, and whether the thread should
	terminate after finishing actions */

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);

	while (!fuse_session_exited(mt->se)) {
		int isforget = 0;
		struct fuse_chan *ch = mt->prevch;
		struct fuse_buf fbuf = {
			.mem = w->buf,
			.size = w->bufsize,
		};
		int res;

		//pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		w->cancelable = 1;
		if (w->terminating == 1)
			pthread_exit(0);
		res = fuse_session_receive_buf(mt->se, &fbuf, &ch);
		w->cancelable = 0;
		//pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		if (res == -EINTR)
			continue;
		if (res <= 0) {
			/* 1/15/2016: Jiahong
			   Do not break off FUSE loop if error occurs.
			   This was intended for catching perm error from
			   kernel due to mount call not readied.
			   TODO: Distinguish error occurred before completion
			   of mount call and the ones during FUSE op */
			if (res < 0)
				sleep(1);
			continue;
			if (res < 0) {
				fuse_session_exit(mt->se);
				mt->error = -1;
			}
			break;
		}

		pthread_mutex_lock(&mt->lock);
		if (mt->exit) {
			pthread_mutex_unlock(&mt->lock);
			return NULL;
		}

		/*
		 * This disgusting hack is needed so that zillions of threads
		 * are not created on a burst of FORGET messages
		 */
		if (!(fbuf.flags & FUSE_BUF_IS_FD)) {
			struct fuse_in_header *in = fbuf.mem;

			if (in->opcode == FUSE_FORGET ||
			    in->opcode == FUSE_BATCH_FORGET)
				isforget = 1;
		}

		if (!isforget)
			mt->numavail--;
		if (mt->numavail == 0)
			fuse_loop_start_thread(mt);
		pthread_mutex_unlock(&mt->lock);

		fuse_session_process_buf(mt->se, &fbuf, ch);

		pthread_mutex_lock(&mt->lock);
		if (!isforget)
			mt->numavail++;
		if (mt->numavail > 10) {
			if (mt->exit) {
				pthread_mutex_unlock(&mt->lock);
				return NULL;
			}
			list_del_worker(w);
			mt->numavail--;
			mt->numworker--;
			pthread_mutex_unlock(&mt->lock);

			pthread_detach(w->thread_id);
			free(w->buf);
			free(w);
			return NULL;
		}
		pthread_mutex_unlock(&mt->lock);
	}

	sem_post(&mt->finish);

	return NULL;
}

int fuse_start_thread(pthread_t *thread_id, void *(*func)(void *), void *arg)
{
	sigset_t oldset;
	sigset_t newset;
	int res;
	pthread_attr_t attr;
	char *stack_size;

	/* Override default stack size */
	pthread_attr_init(&attr);
	stack_size = getenv(ENVNAME_THREAD_STACK);
	if (stack_size && pthread_attr_setstacksize(&attr, atoi(stack_size)))
		fprintf(stderr, "fuse: invalid stack size: %s\n", stack_size);

	/* Disallow signal reception in worker threads */
	sigemptyset(&newset);
	sigaddset(&newset, SIGTERM);
	sigaddset(&newset, SIGINT);
	sigaddset(&newset, SIGHUP);
	sigaddset(&newset, SIGQUIT);
	pthread_sigmask(SIG_BLOCK, &newset, &oldset);
	res = pthread_create(thread_id, &attr, func, arg);
	pthread_sigmask(SIG_SETMASK, &oldset, NULL);
	pthread_attr_destroy(&attr);
	if (res != 0) {
		fprintf(stderr, "fuse: error creating thread: %s\n",
			strerror(res));
		return -1;
	}

	return 0;
}

static int fuse_loop_start_thread(struct fuse_mt *mt)
{
	int res;
	struct fuse_worker *w = malloc(sizeof(struct fuse_worker));
	if (!w) {
		fprintf(stderr, "fuse: failed to allocate worker structure\n");
		return -1;
	}
	memset(w, 0, sizeof(struct fuse_worker));
	w->bufsize = fuse_chan_bufsize(mt->prevch);
	w->buf = malloc(w->bufsize);
	w->mt = mt;
	if (!w->buf) {
		fprintf(stderr, "fuse: failed to allocate read buffer\n");
		free(w);
		return -1;
	}

	w->cancelable = 1;
	w->terminating = 0;
	res = fuse_start_thread(&w->thread_id, fuse_do_work, w);
	if (res == -1) {
		free(w->buf);
		free(w);
		return -1;
	}
	list_add_worker(w, &mt->main);
	mt->numavail ++;
	mt->numworker ++;

	return 0;
}

static void fuse_join_worker(struct fuse_mt *mt, struct fuse_worker *w)
{
	pthread_join(w->thread_id, NULL);
	pthread_mutex_lock(&mt->lock);
	list_del_worker(w);
	pthread_mutex_unlock(&mt->lock);
	free(w->buf);
	free(w);
}

int fuse_session_loop_mt(struct fuse_session *se)
{
	int err;
	struct fuse_mt mt;
	struct fuse_worker *w;

	memset(&mt, 0, sizeof(struct fuse_mt));
	mt.se = se;
	mt.prevch = fuse_session_next_chan(se, NULL);
	mt.error = 0;
	mt.numworker = 0;
	mt.numavail = 0;
	mt.main.thread_id = pthread_self();
	mt.main.prev = mt.main.next = &mt.main;
	sem_init(&mt.finish, 0, 0);
	fuse_mutex_init(&mt.lock);

	pthread_mutex_lock(&mt.lock);
	err = fuse_loop_start_thread(&mt);
	pthread_mutex_unlock(&mt.lock);

	/* Set action handler for SIGUSR1 */
	sighandler_init(&thread_exit_handler);

	/* Init key sigkey */
	pthread_key_create(&sigkey, NULL);

	/* Won't allow threads to receive SIGUSR1 until
	we have initialized sigkey for each thread */
	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);

	if (!err) {
		/* sem_wait() is interruptible */
		/* 2/22/16 it seems that on Android sem_wait may
		not be interruptible */
		while (!fuse_session_exited(se))
			sleep(3600);  /* Sleep for a long time */

		pthread_mutex_lock(&mt.lock);
#if 0
		for (w = mt.main.next; w != &mt.main; w = w->next)
			pthread_cancel(w->thread_id);
#else
		for (w = mt.main.next; w != &mt.main; w = w->next)
			pthread_kill(w->thread_id, SIGUSR1);
#endif
		mt.exit = 1;
		pthread_mutex_unlock(&mt.lock);

		while (mt.main.next != &mt.main)
			fuse_join_worker(&mt, mt.main.next);

		err = mt.error;
	}

	pthread_mutex_destroy(&mt.lock);
	sem_destroy(&mt.finish);
	fuse_session_reset(se);
	return err;
}

#endif
