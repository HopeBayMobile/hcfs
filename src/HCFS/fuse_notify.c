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

FUSE_NOTIFY_RING_BUF notify_buf = {0};
fuse_notify_fn *notify_fn[] = {_do_hfuse_ll_notify_noop,
			       _do_hfuse_ll_notify_delete};

/* Helper functions */

static inline BOOL notify_buf_isempty(void) { return (notify_buf.len == 0); }
static inline BOOL notify_buf_isfull(void)
{
	return notify_buf.len == notify_buf.max_len;
}

/* Increase index in ring buffer
 *
 * @param x pointer to original ring buffer index
 * @param buf_size current size of ring buffer
 */
static inline void inc_buf_idx(size_t *x, size_t buf_size)
{
	(*x)++;
	if ((*x) == buf_size)
		(*x) = 0;
}
static inline BOOL check_buf_init_state(const char *func)
{
	if (notify_buf.is_initialized == 0)
		write_log(1, "Error %s: notify_buf is uninitialized\n", func);
	return notify_buf.is_initialized;
}

/* Initialize fuse notify ring buffer
 *
 * @return 0 on success, otherwise -1.
 */
int32_t init_notify_buf(void)
{
	BOOL good = 0;

	errno = 0;
#define STATIC_CHECK_BUF_SIZE(X)                                               \
	_Static_assert(sizeof(X) <= FUSE_NOTIFY_BUF_ELEMSIZE,                  \
		       "FUSE_NOTIFY_BUF_ELEMSFIZE is not enough for " #X)
	STATIC_CHECK_BUF_SIZE(FUSE_NOTIFY_DELETE_DATA);

	if (notify_buf.is_initialized == 1)
		return 0;
	do {
		notify_buf.max_len = FUSE_NOTIFY_BUF_DEFAULT_LEN;
		notify_buf.in = notify_buf.max_len - 1;
		notify_buf.elems = (void *)malloc(notify_buf.max_len *
						  sizeof(FUSE_NOTIFY_DATA));
		if (notify_buf.elems == NULL)
			break;

		if (sem_init(&notify_buf.tasks_sem, 1, 0) == -1)
			break;

		if (sem_init(&notify_buf.access_sem, 1, 1) == -1)
			break;

		notify_buf.is_initialized = TRUE;
		good = 1;
		write_log(10, "Debug %s: Succeed. max %lu\n", __func__,
			  notify_buf.max_len);
	} while (0);

	if (!good) {
		free(notify_buf.elems);
		notify_buf.elems = NULL;
		write_log(1, "Error %s: Failed. %s\n", __func__,
			  strerror(errno));
	}
	return good ? 0 : -1;
}

void destory_notify_buf(void)
{
	size_t len;
	FUSE_NOTIFY_PROTO *data;

	if (notify_buf.is_initialized == 0)
		return;

	sem_wait(&notify_buf.access_sem);

	/* call _do_hfuse_ll_notify_XXX to free nested member */
	for (len = 1; len <= notify_buf.len; len++) {

		data = (FUSE_NOTIFY_PROTO *)&notify_buf.elems[notify_buf.out];
		/* call destroy action of each data */
		notify_fn[data->func]((FUSE_NOTIFY_DATA **)&data, DESTROY_BUF);
		inc_buf_idx(&notify_buf.out, notify_buf.max_len);
	}
	/* free notify_buf member */
	free(notify_buf.elems);
	notify_buf.elems = NULL;

	notify_buf.is_initialized = FALSE;

	sem_post(&notify_buf.access_sem);

	write_log(10, "Debug %s: Freed notify_buf.elems\n", __func__);
}

/* Concatenate wrapped segments after enlarge buffer
 * +------------------------+
 * |---->|    |<-data-------|
 * +------------------------+
 *       ^    ^
 *       |    |
 *       in   out
 *
 * +------------------------+-----------------------+
 * |          |<-data----------->|                  |
 * +------------------------------------------------+
 *            ^                  ^
 *            |                  |
 *            out                in
 *
 * @return 0 on success; on error, -1 is returned, and errno is set to
 * indicate the error.
 */
int32_t notify_buf_realloc(void)
{
	BOOL good = 0;
	int32_t save_errno;
	void *newbuf = NULL;
	size_t newsize = 0;

	do {
		if (check_buf_init_state(__func__) == 0)
			break;

		/* enlarge buffer */
		newsize = notify_buf.max_len * 2;
		if (newsize > FUSE_NOTIFY_BUF_MAX_LEN) {
			errno = ENOMEM;
			break;
		}
		newbuf = realloc(notify_buf.elems,
				 newsize * sizeof(FUSE_NOTIFY_DATA));
		if (newbuf == NULL)
			break;
		notify_buf.elems = newbuf;

		/* Concatenate wrapped segments */
		if (notify_buf.in < notify_buf.out) {
			memcpy(&notify_buf.elems[notify_buf.max_len],
			       notify_buf.elems,
			       (notify_buf.in + 1) * sizeof(FUSE_NOTIFY_DATA));
			notify_buf.in += notify_buf.max_len;
		}

		notify_buf.max_len = newsize;
		good = 1;
		write_log(
		    10,
		    "Debug %s: Notification queue is resized to %lu\n",
		    __func__, notify_buf.max_len);
	} while (0);
	save_errno = errno;

	if (!good) {
		if (newsize > FUSE_NOTIFY_BUF_MAX_LEN)
			write_log(1,
				  "Error %s: Queue limit %d reached\n",
				  __func__, FUSE_NOTIFY_BUF_MAX_LEN);
		else
			write_log(1, "Error %s: Failed\n", __func__);
		errno = save_errno;
	}
	return good ? 0 : -1;
}
/* @param notify: A pointer of data to be added into notify queue
 *
 * @return 0 on success; on error, -1 is returned, and errno is set to
 * indicate the error.
 */
int32_t notify_buf_enqueue(const void *const notify)
{
	BOOL good = 0;
	int32_t save_errno;

	do {
		if (check_buf_init_state(__func__) == 0)
			break;

		sem_wait(&notify_buf.access_sem);

		if (notify_buf_isfull() == 1 && notify_buf_realloc() == -1)
			break;
		inc_buf_idx(&notify_buf.in, notify_buf.max_len);
		memcpy(&notify_buf.elems[notify_buf.in], notify,
		       sizeof(FUSE_NOTIFY_DATA));
		notify_buf.len++;

		sem_post(&notify_buf.access_sem);

		good = 1;
		write_log(10, "Debug %s: Add %lu\n", __func__, notify_buf.in);
	} while (0);
	save_errno = errno;

	if (!good) {
		write_log(1, "Error %s: Failed\n", __func__);
		errno = save_errno;
	}
	return good ? 0 : -1;
}

/* Dequeue the notify ring buffer
 *
 * @return Pointer on success, NULL on fail with errno set.
 */
FUSE_NOTIFY_DATA *notify_buf_dequeue()
{
	FUSE_NOTIFY_DATA *data = NULL;
	BOOL buf_isempty;

	do {
		if (check_buf_init_state(__func__) == 0)
			break;
		sem_wait(&notify_buf.access_sem);

		buf_isempty = notify_buf_isempty();
		if (buf_isempty == 1) {
			write_log(1, "Error %s: Queue is empty.\n", __func__);
			break;
		}

		data = malloc(sizeof(FUSE_NOTIFY_DATA));
		if (data == NULL) {
			write_log(1, "Error %s: malloc failed.\n", __func__);
			break;
		}

		memcpy(data, &notify_buf.elems[notify_buf.out],
		       sizeof(FUSE_NOTIFY_DATA));
		inc_buf_idx(&notify_buf.out, notify_buf.max_len);
		notify_buf.len--;

		sem_post(&notify_buf.access_sem);
		write_log(10, "Debug %s: Get %lu\n", __func__, notify_buf.out);
	} while (0);

	return data;
}

/* Initialize notify_buf and create new thread to loop reading buffer and
 * send notify to VFS.
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t init_hfuse_ll_notify_loop(void)
{
	BOOL good = 0;
	int32_t save_errno = 0;

	/* init enviroments */
	do {
		if (init_notify_buf() == -1)
			break;

		save_errno = pthread_create(&fuse_nofify_thread, NULL,
					    hfuse_ll_notify_loop, NULL);
		if (save_errno != 0)
			break;
		good = 1;
		write_log(10, "Debug %s: Succeed\n", __func__);
	} while (0);

	if (!good) {
		write_log(1, "%s failed\n", __func__);
		errno = save_errno;
	}

	return good ? 0 : -1;
}

/* Wake notify loop and wait the thread end */
void destory_hfuse_ll_notify_loop(void)
{
	int32_t save_errno = 0;

	write_log(10, "Debug %s: Start.\n", __func__);
	/* let loop handle destory tasks itself */
	if (sem_post(&notify_buf.tasks_sem) == -1) {
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
	FUSE_NOTIFY_DATA *data;
	int32_t func_num;

	UNUSED(ptr);
	write_log(10, "Debug %s: Start\n", __func__);

	while (TRUE) {
		sem_wait(&notify_buf.tasks_sem);
		if (hcfs_system->system_going_down == TRUE)
			break;
		data = notify_buf_dequeue();
		if (data != NULL) {
			func_num = ((FUSE_NOTIFY_PROTO *)data)->func;
			notify_fn[func_num](&data, RUN);
			write_log(10, "Debug %s: Notified\n", __func__);
		} else {
			write_log(1, "Error %s: Dequeue failed\n", __func__);
		}
	}

	write_log(1, "Debug %s: Loop end\n", __func__);
	return NULL;
}

/* START -- Functions running in notify thread */

/* A noop funtion in case the loop thread call with empty data if some
 * unexpected error happend. noop will free when not
 *     exexute with DESTROY_BU mode.
 *F
 * @param data_ptr pointer to FUSE_NOTIFY_DATA
 * @param action what action will be execute.
 */
void _do_hfuse_ll_notify_noop(FUSE_NOTIFY_DATA **data_ptr,
			      enum NOTIFY_ACTION action)
{
	if (action != DESTROY_BUF) {
		free(*data_ptr);
		*data_ptr = NULL;
		write_log(10, "Debug %s: Free notify data\n", __func__);
	}
}

/* Actuall function to call libfuse notify. If action == DESTROY_BUF
 * it means this function is called to destroy nested data pointed by
 * ring buffer, so the pointer is part of buf and will not be freed
 * directly.
 *
 * @param data_ptr pointer to FUSE_NOTIFY_DATA
 * @param action what action will be execute.
 */
void _do_hfuse_ll_notify_delete(FUSE_NOTIFY_DATA **data_ptr,
				enum NOTIFY_ACTION action)
{
	FUSE_NOTIFY_DELETE_DATA *data = *((FUSE_NOTIFY_DELETE_DATA **)data_ptr);

	if (action == RUN) {
		fuse_lowlevel_notify_delete(data->ch, data->parent, data->child,
					    data->name, data->namelen);
		write_log(10, "Debug %s: Called\n", __func__);
	}

	/* free nested member */
	free(data->name);
	data->name = NULL;
	/* struct fuse_chan is shared, should not be freed here */

	/* If it's not recycling notify_buf, ptr is copied in
	 * notify_buf_dequeue. It need to be destroyed.
	 */
	if (action != DESTROY_BUF) {
		free(*data_ptr);
		*data_ptr = NULL;
		write_log(10, "Debug %s: Free notify data\n", __func__);
	}
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
 * @return 0 on success; on error, -1 is returned, and errno is set to
 * indicate the error.
 */
int32_t hfuse_ll_notify_delete(struct fuse_chan *ch,
			    fuse_ino_t parent,
			    fuse_ino_t child,
			    const char *name,
			    size_t namelen)
{
	BOOL good = 0;
	int32_t save_errno;
	FUSE_NOTIFY_DELETE_DATA event = {.func = DELETE,
					 .ch = ch,
					 .parent = parent,
					 .child = child,
					 .name = NULL,
					 .namelen = namelen};
	do {
		event.name = strndup(name, namelen);
		if (event.name == NULL)
			break;
		if (notify_buf_enqueue((void *)&event) == -1)
			break;

		/* wake notify thread up */
		if (sem_post(&notify_buf.tasks_sem) == -1)
			break;
		good = 1;
		write_log(10, "Debug %s: New notify is queued\n", __func__);
	} while (0);
	save_errno = errno;

	if (!good) {
		write_log(1, "Error %s: Failed\n", __func__);
		if (event.name != NULL)
			free(event.name);
		errno = save_errno;
	}

	return good ? 0 : -1;
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
 * @return 0 on success; on error, -1 is returned, and errno is set to
 * indicate the error.
 */
int32_t hfuse_ll_notify_delete_mp(struct fuse_chan *ch,
			       fuse_ino_t parent,
			       fuse_ino_t child,
			       const char *name,
			       size_t namelen,
			       const char *selfname)
{
	BOOL good;
	int32_t save_errno;
	BOOL samecase = (strcmp(name, selfname) == 0);
	int32_t i;

	write_log(10, "Debug %s: Called\n", __func__);
	good = 1;
	for (i = 1; i <= MP_TYPE_NUM && good == 1; i++) {
		if (mount_global.ch[i] == NULL)
			continue;
		/* no need to notify if filename in same case, same view */
		if (samecase && mount_global.ch[i] == ch)
			continue;
		if (hfuse_ll_notify_delete(mount_global.ch[i], parent, child,
					   name, namelen) == -1)
			good = 0;
	}
	save_errno = errno;

	if (!good) {
		write_log(1, "Error %s: Failed\n", __func__);
		errno = save_errno;
	}
	return good ? 0 : -1;
}
/* END -- Functions running in fuse operation */
