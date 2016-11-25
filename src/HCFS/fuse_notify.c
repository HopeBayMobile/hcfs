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

FUES_NOTIFY_SHARED_DATA notify;
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
	if ((*x) == FUSE_NOTIFY_RINGBUF_SIZE)
		(*x) = 0;
}

/* Initialize fuse notify ring buffer
 *
 * @return zero for success, -errno for failure
 */
int32_t init_notify_buf(void)
{
	int32_t ret = -1;

	errno = 0;
	memset(&notify, 0, sizeof(FUES_NOTIFY_SHARED_DATA));
	notify.in = FUSE_NOTIFY_RINGBUF_SIZE - 1;
	notify.out = 0;
	notify.len = 0;
	do {
		if (sem_init(&notify.not_empty, 1, 0) == -1)
			break;
		if (sem_init(&notify.access_sem, 1, 1) == -1)
			break;

		write_log(10, "Debug %s: Succeed.\n", __func__);
		ret = 0;
		break;
	} while (0);

	if (ret < 0) {
		if (errno != 0)
			ret = -errno;
		write_log(0, "Error %s: Failed. %s\n", __func__,
			  errno ? strerror(errno) : "");
	}
	return ret;
}

void destory_notify_buf(void)
{
	size_t len, idx;
	FUSE_NOTIFY_PROTO *proto;

	idx = notify.out;
	/* Free member in notify data */
	for (len = 1; len <= notify.len; len++) {
		proto = (FUSE_NOTIFY_PROTO *)notify.ring_buf[idx];
		notify_fn[proto->func](proto, DESTROY_BUF);
		inc_buf_idx(&idx);
	}

	notify.in = 0;
	notify.out = 0;
	notify.len = 0;
	sem_destroy(&notify.access_sem);
	sem_destroy(&notify.not_empty);

	write_log(10, "Debug %s: Done\n", __func__);
}

/*
 * Enqueue notify data to an extra linked list. It's called when ring
 * buffer is full.
 */
int32_t _ll_enqueue(const void *const data)
{
	FUSE_NOTIFY_LL *node = malloc(sizeof(FUSE_NOTIFY_LL));

	if (node == NULL)
		return -1;
	node->data = malloc(FUSE_NOTIFY_ENTRY_SIZE);
	if (node->data == NULL) {
		FREE(node);
		return -1;
	}

	/* Fill data */
	node->next = NULL;
	memcpy(node->data, data, FUSE_NOTIFY_ENTRY_SIZE);

	/* Chain ndoe */
	if (notify.extend_ll_head == NULL) {
		notify.extend_ll_head = node;
		notify.extend_ll_rear = node;
	} else {
		notify.extend_ll_rear->next = node;
		notify.extend_ll_rear = node;
	}

	return 0;
}
int32_t _rb_enqueue(const void *const data)
{
	inc_buf_idx(&notify.in);
	memcpy(&notify.ring_buf[notify.in], data, FUSE_NOTIFY_ENTRY_SIZE);
	return 0;
}
/* @param notify: A pointer of data to be added into notify queue
 *
 * @return zero for success, -errno for failure
 */
int32_t notify_buf_enqueue(const void *const data)
{
	int32_t ret = -1;

	errno = 0;
	while (hcfs_system->system_going_down == FALSE) {
		sem_wait(&notify.access_sem);

		/* Fuse has multiple threads. Enqueue need to be thread safe */
		ret = (notify.len >= FUSE_NOTIFY_RINGBUF_SIZE)
			  ? _ll_enqueue(data)
			  : _rb_enqueue(data);
		if (ret < 0)
			break;
		notify.len++;

		/* wake notify thread up */
		if (notify.len == 1)
			sem_post(&notify.not_empty);

		write_log(10, "Debug %s: Add %lu\n", __func__, notify.in);
		ret = 0;
		break;
	};
	sem_post(&notify.access_sem);

	if (ret < 0)
		write_log(0, "Error %s: Failed. %s\n", __func__,
			  errno ? strerror(errno) : "");
	return ret;
}

FUSE_NOTIFY_PROTO *_ll_dequeue(void)
{
	FUSE_NOTIFY_LL *node;
	FUSE_NOTIFY_PROTO *data = NULL;

	if (notify.extend_ll_head == NULL)
		return NULL;

	node = notify.extend_ll_head;
	notify.extend_ll_head = notify.extend_ll_head->next;
	if (notify.extend_ll_head == NULL)
		notify.extend_ll_rear = NULL;

	data = node->data;
	FREE(node);
	return data;
}
FUSE_NOTIFY_PROTO *_rb_dequeue(void)
{
	FUSE_NOTIFY_PROTO *data = NULL;

	data = malloc(FUSE_NOTIFY_ENTRY_SIZE);
	if (data == NULL)
		return NULL;

	memcpy(data, &notify.ring_buf[notify.out], FUSE_NOTIFY_ENTRY_SIZE);
	write_log(10, "Debug %s: Get %lu\n", __func__, notify.out);
	inc_buf_idx(&notify.out);
	return data;
}
/* Dequeue the notify ring buffer
 *
 * @return Pointer on success, NULL on fail with errno set.
 */
FUSE_NOTIFY_PROTO *notify_buf_dequeue()
{
	FUSE_NOTIFY_PROTO *data = NULL;

	errno = 0;
	while (hcfs_system->system_going_down == FALSE) {
		sem_wait(&notify.access_sem);
		if (notify.len == 0) {
			sem_post(&notify.access_sem);
			sem_wait(&notify.not_empty);
			continue;
		}

		data = _rb_dequeue();
		if (data == NULL)
			break;
		notify.len--;

		/* Try to move one notify from linked list to ring buffer */
		if (notify.len < FUSE_NOTIFY_RINGBUF_SIZE) {
			FUSE_NOTIFY_PROTO *tmp_notify = _ll_dequeue();

			if (tmp_notify != NULL)
				_rb_enqueue(tmp_notify);
			FREE(tmp_notify);
		}
		break;
	}
	sem_post(&notify.access_sem);

	return data;
}

/* Initialize notify and create new thread to loop reading buffer and
 * send notify to VFS.
 *
 * @return zero for success, -errno for failure
 */
int32_t init_hfuse_ll_notify_loop(void)
{
	int32_t ret;

	/* init enviroments */
	while (TRUE) {
		ret = init_notify_buf();
		if (ret < 0)
			break;

		ret = -pthread_create(&fuse_nofify_thread, NULL,
				      hfuse_ll_notify_loop, NULL);
		if (ret < 0)
			break;
		ret = 0;
		write_log(10, "Debug %s: Succeed\n", __func__);
		break;
	}

	if (ret < 0)
		write_log(1, "Error %s: Failed. %s\n", __func__,
			  strerror(-ret));
	return ret;
}

/* Wake notify loop and wait the thread end */
int32_t destory_hfuse_ll_notify_loop(void)
{
	int32_t save_errno = 0;

	write_log(10, "Debug %s: Start.\n", __func__);
	/* let loop handle destory tasks itself */
	if (sem_post(&notify.not_empty) == -1) {
		save_errno = errno;
		write_log(1, "Error %s: sem_post failed. %s\n",
			  __func__, strerror(save_errno));
		return -save_errno;
	}
	save_errno = pthread_join(fuse_nofify_thread, NULL);
	if (save_errno != 0) {
		write_log(1, "Error %s: Failed to join nofify_thread. %s\n",
			  __func__, strerror(save_errno));
		return -save_errno;
	}

	/* destory enviroments */
	destory_notify_buf();
	return save_errno;
}

void *hfuse_ll_notify_loop(void *ptr)
{
	FUSE_NOTIFY_PROTO *data = NULL;
	int32_t func_num;
	int32_t save_errno = 0;

	UNUSED(ptr);
	write_log(10, "Debug %s: Start\n", __func__);

	errno = 0;
	while (TRUE) {
		data = notify_buf_dequeue();
		if (hcfs_system->system_going_down == TRUE)
			break;
		if (data != NULL) {
			func_num = data->func;
			notify_fn[func_num](data, RUN);
			write_log(10, "Debug %s: Notification is sent.\n",
				  __func__);
			free(data);
		} else {
			save_errno = errno;
			write_log(1, "Error %s: Dequeue failed. %s\n", __func__,
				  strerror(save_errno));
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
 * @param data_ptr pointer to FUSE_NOTIFY_PROTO
 * @param action what action will be execute.

 * @return zero.
 */
int32_t _do_hfuse_ll_notify_noop(_UNUSED FUSE_NOTIFY_PROTO *data,
				 _UNUSED enum NOTIFY_ACTION action)
{
	write_log(10, "Debug %s: Do nothing.\n", __func__);
	return 0;
}

/* Actuall function to call libfuse notify; If action == DESTROY_BUF,
 * function will only free nested data structure.
 *
 * @param data_ptr pointer to FUSE_NOTIFY_PROTO
 * @param action what action will be execute.
 *
 * @return zero for success, -errno for failure
 */
int32_t _do_hfuse_ll_notify_delete(FUSE_NOTIFY_PROTO *data,
				enum NOTIFY_ACTION action)
{
	int32_t ret = 0;
	FUSE_NOTIFY_DELETE_DATA *del = (FUSE_NOTIFY_DELETE_DATA *)data;

	if (action == RUN)
		ret = fuse_lowlevel_notify_delete(
		    del->ch, del->parent, del->child, del->name, del->namelen);
	/* Don't error if notify_delete non-existed entries */
	if (ret == -ENOENT)
		ret = 0;

	/* Free members. Don't free fuse_chan since it's shared */
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
	while (TRUE) {
		event.name = strndup(name, namelen);
		if (event.name == NULL) {
			ret = -errno;
			break;
		}
		ret = notify_buf_enqueue((void *)&event);
		if (ret < 0)
			break;
		write_log(10, "Debug %s: New notify is queued\n", __func__);
		break;
	}

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

	if (ret < 0)
		write_log(1, "Error %s: %s\n", __func__, strerror(-ret));
	return ret;
}
/* END -- Functions running in fuse operation */
