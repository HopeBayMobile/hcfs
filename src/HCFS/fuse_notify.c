/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: fuse_notify.c
* Abstract: The c source code file for fuse notification related FUSE
*           operation.
*
* Revision History
* 2016/9/1 Jethro Add notification for deleting file.
*
**************************************************************************/

#define _GNU_SOURCE
#include "fuse_notify.h"

#include <errno.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>

#include "hfuse_system.h"
#include "logger.h"
#include "mount_manager.h"
#ifdef UNITTEST
#include "ut_helper.h"
#endif

FUSE_NOTIFY_RING_BUF notify_buf;
fuse_notify_fn *notify_fn[] = {_do_hfuse_ll_notify_noop,
			       _do_hfuse_ll_notify_delete};

/* Increase index in ring buffer
 *
 * @param x pointer to original ring buffer index
 * @param buf_size current size of ring buffer
 */
static inline void inc_buf_idx(size_t *x)
{
	(*x)++;
	if ((*x) == FUSE_NOTIFY_BUF_MAX_LEN)
		(*x) = 0;
}

/* Initialize fuse notify ring buffer
 *
 * @return zero for success, -errno for failure
 */
int32_t init_notify_buf(void)
{
	int32_t ret = -1;

#define STATIC_CHECK_BUF_SIZE(X)                                               \
	_Static_assert(sizeof(X) <= FUSE_NOTIFY_BUF_ELEMSIZE,                  \
		       "FUSE_NOTIFY_BUF_ELEMSFIZE is not enough for " #X)
	STATIC_CHECK_BUF_SIZE(FUSE_NOTIFY_DELETE_DATA);

	errno = 0;
	do {
		memset(&notify_buf, 0, sizeof(FUSE_NOTIFY_RING_BUF));
		notify_buf.in = FUSE_NOTIFY_BUF_MAX_LEN - 1;
		notify_buf.out = 0;
		notify_buf.len = 0;

		if (sem_init(&notify_buf.not_empty, 1, 0) == -1)
			break;
		if (sem_init(&notify_buf.not_full, 1, 0) == -1)
			break;
		if (sem_init(&notify_buf.access_sem, 1, 1) == -1)
			break;

		ret = 0;
		write_log(10, "Debug %s: Succeed.\n", __func__);
	} while (0);
	if (ret < 0 && errno != 0)
		ret = -errno;

	if (ret < 0) {
		write_log(1, "Error %s: Failed. %s\n", __func__,
			  strerror(-ret));
	}
	return ret;
}

void destory_notify_buf(void)
{
	size_t len, idx;
	FUSE_NOTIFY_PROTO *proto;
	FUSE_NOTIFY_DATA *data;

	idx = notify_buf.out;
	/* Free member in notify data */
	for (len = 1; len <= notify_buf.len; len++) {
		proto = (FUSE_NOTIFY_PROTO *)&notify_buf.elems[idx];
		data = &notify_buf.elems[idx];
		notify_fn[proto->func](data, DESTROY_BUF);
		inc_buf_idx(&idx);
	}

	notify_buf.in = 0;
	notify_buf.out = 0;
	notify_buf.len = 0;
	sem_destroy(&notify_buf.access_sem);
	sem_destroy(&notify_buf.not_empty);
	sem_destroy(&notify_buf.not_full);

	write_log(10, "Debug %s: Done\n", __func__);
}

/* @param notify: A pointer of data to be added into notify queue
 *
 * @return zero for success, -errno for failure
 */
int32_t notify_buf_enqueue(const void *const notify)
{
	int32_t ret = -1;

	do {
		if (hcfs_system->system_going_down == TRUE)
			break;
		/* Fuse has multiple threads. Enqueue need to be thread safe */
		sem_wait(&notify_buf.access_sem);

		if (notify_buf.len == FUSE_NOTIFY_BUF_MAX_LEN) {
			sem_post(&notify_buf.access_sem);
			sem_wait(&notify_buf.not_full);
			continue;
		}

		inc_buf_idx(&notify_buf.in);
		memcpy(&notify_buf.elems[notify_buf.in], notify,
		       sizeof(FUSE_NOTIFY_DATA));
		notify_buf.len++;

		sem_post(&notify_buf.access_sem);

		/* wake notify thread up */
		if (notify_buf.len == 1)
			sem_post(&notify_buf.not_empty);

		ret = 0;
		write_log(10, "Debug %s: Add %lu\n", __func__, notify_buf.in);
	} while (0);

	if (ret < 0)
		write_log(1, "Error %s: Failed. %s\n", __func__,
			  strerror(-ret));

	return ret;
}

/* Dequeue the notify ring buffer
 *
 * @return Pointer on success, NULL on fail with errno set.
 */
FUSE_NOTIFY_DATA *notify_buf_dequeue()
{
	FUSE_NOTIFY_DATA *data = NULL;

	errno = 0;
	do {
		if (hcfs_system->system_going_down == TRUE)
			break;
		if (notify_buf.len == 0) {
			sem_wait(&notify_buf.not_empty);
			continue;
		}

		data = malloc(sizeof(FUSE_NOTIFY_DATA));
		if (data == NULL)
			break;

		memcpy(data, &notify_buf.elems[notify_buf.out],
		       sizeof(FUSE_NOTIFY_DATA));
		write_log(10, "Debug %s: Get %lu\n", __func__, notify_buf.out);
		inc_buf_idx(&notify_buf.out);
		notify_buf.len--;

		/* wake fuse IO thread up */
		if (notify_buf.len == (FUSE_NOTIFY_BUF_MAX_LEN - 1))
			sem_post(&notify_buf.not_full);
	} while (0);
	if (data == NULL && errno == 0)
		errno = EPERM;

	if (data == NULL)
		write_log(1, "Error %s: Failed. %s\n", __func__,
			  strerror(errno));
	return data;
}

/* Initialize notify_buf and create new thread to loop reading buffer and
 * send notify to VFS.
 *
 * @return zero for success, -errno for failure
 */
int32_t init_hfuse_ll_notify_loop(void)
{
	int32_t ret;

	/* init enviroments */
	do {
		ret = init_notify_buf();
		if (ret < 0)
			break;

		ret = -pthread_create(&fuse_nofify_thread, NULL,
				      hfuse_ll_notify_loop, NULL);
		if (ret < 0)
			break;
		ret = 0;
		write_log(10, "Debug %s: Succeed\n", __func__);
	} while (0);

	if (ret < 0)
		write_log(1, "Error %s: Failed. %s\n", __func__,
			  strerror(-ret));
	return ret;
}

/* Wake notify loop and wait the thread end */
void destory_hfuse_ll_notify_loop(void)
{
	int32_t save_errno = 0;

	write_log(10, "Debug %s: Start.\n", __func__);
	/* let loop handle destory tasks itself */
	if (sem_post(&notify_buf.not_empty) == -1) {
		write_log(1, "Error %s: sem_post failed. %s\n",
			  __func__, strerror(save_errno));
		return;
	}
	save_errno = pthread_join(fuse_nofify_thread, NULL);
	if (save_errno != 0) {
		write_log(1, "Error %s: Failed to join nofify_thread. %s\n",
			  __func__, strerror(save_errno));
		return;
	}

	/* destory enviroments */
	destory_notify_buf();
}

void *hfuse_ll_notify_loop(void *ptr)
{
	FUSE_NOTIFY_DATA *data = NULL;
	int32_t func_num;

	UNUSED(ptr);
	write_log(10, "Debug %s: Start\n", __func__);

	while (TRUE) {
		data = notify_buf_dequeue();
		if (hcfs_system->system_going_down == TRUE)
			break;
		if (data != NULL) {
			func_num = ((FUSE_NOTIFY_PROTO *)data)->func;
			notify_fn[func_num](data, RUN);
			write_log(10, "Debug %s: Notification is sent.\n",
				  __func__);
			free(data);
		} else {
			write_log(1, "Error %s: Dequeue failed\n", __func__);
		}
	}

	free(data);

	write_log(10, "Debug %s: Loop end\n", __func__);
	return NULL;
}

/* START -- Functions running in notify thread */

/* A noop funtion in case the loop thread call with empty data if some
 * unexpected error happend. noop will free when not
 *     exexute with DESTROY_BU mode.
 *
 * @param data_ptr pointer to FUSE_NOTIFY_DATA
 * @param action what action will be execute.

 * @return zero.
 */
int32_t _do_hfuse_ll_notify_noop(_UNUSED FUSE_NOTIFY_DATA *data,
				 _UNUSED enum NOTIFY_ACTION action)
{
	write_log(10, "Debug %s: Do nothing.\n", __func__);
	return 0;
}

/* Actuall function to call libfuse notify; If action == DESTROY_BUF,
 * function will only free nested data structure.
 *
 * @param data_ptr pointer to FUSE_NOTIFY_DATA
 * @param action what action will be execute.
 *
 * @return zero for success, -errno for failure
 */
int32_t _do_hfuse_ll_notify_delete(FUSE_NOTIFY_DATA *data,
				enum NOTIFY_ACTION action)
{
	int32_t ret = 0;
	FUSE_NOTIFY_DELETE_DATA *del = (FUSE_NOTIFY_DELETE_DATA *)data;

	if (action == RUN)
		ret = fuse_lowlevel_notify_delete(
		    del->ch, del->parent, del->child, del->name, del->namelen);
	/* free member data. struct fuse_chan is shared, should not be
	 * freed here
	 */
	free(del->name);
	del->name = NULL;

	if (ret == 0)
		write_log(10, "Debug %s: Notified Kernel.\n", __func__);
	else
		write_log(1, "Error %s: %s\n", __func__, strerror(-ret));
	return ret;
}
/* END -- Functions running in notify thread */

/* START -- Functions running in fuse operation */

/* Add delete notify task to buffer and wake up loop thread to handle it.
 *
 * @param ch Fuse channel to communicate with.
 * @param parent Inode of deleted file's parent.
 * @param child Inode of deleted file itself.
 * @param name Name of deleted file.
 * @param namelen Length of deleted filename.
 *
 * @return zero for success, -errno for failure
 */
int32_t hfuse_ll_notify_delete(struct fuse_chan *ch,
			    fuse_ino_t parent,
			    fuse_ino_t child,
			    const char *name,
			    size_t namelen)
{
	int32_t ret = 0;
	FUSE_NOTIFY_DELETE_DATA event = {.func = DELETE,
					 .ch = ch,
					 .parent = parent,
					 .child = child,
					 .name = NULL,
					 .namelen = namelen};
	do {
		event.name = strndup(name, namelen);
		if (event.name == NULL) {
			ret = -errno;
			break;
		}
		ret = notify_buf_enqueue((void *)&event);
		if (ret < 0)
			break;
		write_log(10, "Debug %s: New notify is queued\n", __func__);
	} while (0);

	if (ret < 0) {
		write_log(1, "Error %s: %s\n", __func__, strerror(-ret));
		/* recycle on fail */
		if (event.name != NULL)
			free(event.name);
	}

	return ret;
}

/* Dealing fuse notify when given command's filename and actual filename
 * in HCFS. if filename is same, the other 2 views will get extra notify;
 * otherwise all 3 views will be notify with deleted file's actual name.
 *
 * @param ch Fuse channel to communicate with.
 * @param parent Inode of deleted file's parent.
 * @param child Inode of deleted file itself.
 * @param name Name of deleted file.
 * @param namelen Length of deleted filename.
 * @param selfname, The file name comes with fuse op call.
 *
 * @return zero for success, -errno for failure
 * indicate the error.
 */
int32_t hfuse_ll_notify_delete_mp(struct fuse_chan *ch,
			       fuse_ino_t parent,
			       fuse_ino_t child,
			       const char *name,
			       size_t namelen,
			       const char *selfname)
{
	int32_t ret;
	BOOL samecase = (strcmp(name, selfname) == 0);
	int32_t i;

	write_log(10, "Debug %s: Called\n", __func__);

	ret = 0;
	for (i = 1; i <= MP_TYPE_NUM; i++) {
		if (mount_global.ch[i] == NULL)
			continue;
		/* no need to notify if filename in same case, same view */
		if (samecase && mount_global.ch[i] == ch)
			continue;
		ret = hfuse_ll_notify_delete(mount_global.ch[i], parent, child,
					     name, namelen);
		if (ret < 0)
			break;
	}

	if (ret < 0) {
		write_log(1, "Error %s: %s\n", __func__, strerror(-ret));
	}
	return ret;
}
/* END -- Functions running in fuse operation */
