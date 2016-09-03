/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: fuse_notify.c
* Abstract: The c source code file for fuse notification related FUSE
* operation.
*
* Revision History
* 2016/9/1 Jethro Add notification for deleting file.
*
**************************************************************************/

#include "fuse_notify.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "hfuse_system.h"
#include "mount_manager.h"

FUSE_NOTIFY_RING_BUF notify_buf = {0};
fuse_notify_fn *notify_fn[] = {_do_hfuse_ll_notify_noop,
			       _do_hfuse_ll_notify_delete};

/* Helper functions */

#define check_buf_elem_size(X)                                                 \
	_Static_assert(FUSE_NOTIFY_BUF_ELEMSIZE >= sizeof(X),                  \
		       "FUSE_NOTIFY_BUF_ELEMSFIZE is not enough for " #X)
static inline BOOL notify_buf_isempty() { return notify_buf.len == 0; }
static inline BOOL notify_buf_isfull()
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

/* Initialize fuse notify ring buffer
 *
 * @return 0 on success, otherwise -1.
 */
int32_t init_notify_buf(void)
{
	BOOL good = TRUE;
	errno = 0;

	check_buf_elem_size(FUSE_NOTIFY_DELETE_DATA);

	notify_buf.max_len = FUSE_NOTIFY_BUF_DEFAULT_LEN;
	notify_buf.in = notify_buf.max_len - 1;

	notify_buf.elems =
	    (void *)malloc(notify_buf.max_len * sizeof(FUSE_NOTIFY_DATA));
	good = (notify_buf.elems != NULL);

	if (good)
		good = (0 == sem_init(&notify_buf.tasks_sem, 1, 0));
	if (good)
		good = (0 == sem_init(&notify_buf.access_sem, 1, 1));

	if (good) {
		notify_buf.is_initialized = TRUE;
		write_log(4, "Debug %s: succeed. max %lu\n", __func__,
			  notify_buf.max_len);
	} else {
		notify_buf.is_initialized = FALSE;
		free(notify_buf.elems);
		notify_buf.elems = NULL;
		write_log(4, "Debug %s: failed. %s\n", __func__,
			  strerror(errno));
	}
	return good ? 0 : -1;
}

void destory_notify_buf(void)
{
	size_t len;
	FUSE_NOTIFY_PROTO *data;

	if (notify_buf.is_initialized == FALSE)
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

	write_log(4, "Debug %s: freed notify_buf.elems\n", __func__);
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
 * @return 0 on success, otherwise -1.
 */
int32_t notify_buf_realloc(void)
{
	void *newbuf = NULL;
	size_t newsize;
	BOOL good = notify_buf.is_initialized;

	/* enlarge buffer */
	if (good) {
		newsize = notify_buf.max_len * 2;
		newbuf = realloc(notify_buf.elems,
				 newsize * sizeof(FUSE_NOTIFY_DATA));
		good = (newbuf != NULL);
	}

	if (good) {
		notify_buf.elems = newbuf;

		if (notify_buf.in < notify_buf.out) {
			memcpy(&notify_buf.elems[notify_buf.max_len],
			       notify_buf.elems,
			       (notify_buf.in + 1) * sizeof(FUSE_NOTIFY_DATA));
			notify_buf.in += notify_buf.max_len;
		}
		notify_buf.max_len = newsize;
		write_log(4,
			  "Debug %s: queue with %lu notify is realloc to %lu\n",
			  __func__, notify_buf.len, notify_buf.max_len);
	} else {
		write_log(4, "Debug %s: failed\n", __func__);
	}

	return good ? 0 : -1;
}

int32_t notify_buf_enqueue(const void *const notify)
{
	BOOL good = notify_buf.is_initialized;

	if (!good) {
		write_log(4, "Debug %s: notify buffer is not initialized\n",
			  __func__);
		return -1;
	}
	if (good)
		good = (sem_wait(&notify_buf.access_sem) == 0);

	if (good && notify_buf_isfull())
		good = (notify_buf_realloc() == EXIT_SUCCESS);
	if (good) {
		inc_buf_idx(&notify_buf.in, notify_buf.max_len);
		memcpy(&notify_buf.elems[notify_buf.in], notify,
		       sizeof(FUSE_NOTIFY_DATA));
		notify_buf.len++;
	}
	if (good)
		good = (sem_post(&notify_buf.access_sem) == 0);
	if (good)
		write_log(4, "Debug %s: Add %lu\n", __func__, notify_buf.in);
	else
		write_log(4, "Debug %s: Failed\n", __func__);
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
	BOOL good = notify_buf.is_initialized;

	sem_wait(&notify_buf.access_sem);

	if (good)
		good = ((buf_isempty = notify_buf_isempty()) == 0);

	if (good)
		good = ((data = malloc(sizeof(FUSE_NOTIFY_DATA))) != NULL);

	if (good) {
		memcpy(data, &notify_buf.elems[notify_buf.out],
		       sizeof(FUSE_NOTIFY_DATA));
		write_log(4, "Debug %s: Get %lu\n", __func__, notify_buf.out);
		inc_buf_idx(&notify_buf.out, notify_buf.max_len);
		notify_buf.len--;
	}

	sem_post(&notify_buf.access_sem);

	if (good)
		return data;

	if (notify_buf.is_initialized == 0)
		write_log(4, "Debug %s: failed. %s\n", __func__,
			  "Buffer is not initialized.");
	else if (buf_isempty)
		write_log(4, "Debug %s: failed. %s\n", __func__,
			  "Trying to dequeue an empty queue.");
	else
		write_log(4, "Debug %s: failed. %s\n", __func__,
			  "malloc failed.");

	return data;
}

/* Initialize notify_buf and create new thread to loop reading buffer and
 * send notify to VFS.
 *
 * @return 0 on success, otherwise negative error code.
 * */
int32_t init_hfuse_ll_notify_loop(void)
{
	BOOL good = TRUE;
	int32_t ret_val = 0;

	/* init enviroments */
	if (notify_buf.is_initialized == FALSE) {
		ret_val = init_notify_buf();
		good = (0 == ret_val);
	}

	if (good) {
		ret_val = -pthread_create(&fuse_nofify_thread, NULL,
					  hfuse_ll_notify_loop, NULL);
		good = (0 == ret_val);
	}

	if (good)
		write_log(10, "Debug %s: succeed\n", __func__);
	else
		write_log(1, "%s failed\n", __func__);

	return ret_val;
}

/* Wake notify loop and wait the thread end */
void destory_hfuse_ll_notify_loop(void)
{
	BOOL good = TRUE;
	int32_t ret = 0;

	write_log(10, "Debug %s: start.\n", __func__);
	/* let loop handle destory tasks itself */
	sem_post(&notify_buf.tasks_sem);

	good = (ret = pthread_join(fuse_nofify_thread, NULL)) == 0;
	if (!good)
		write_log(1, "Error %s: join nofify_thread failed. %s\n",
			  __func__, strerror(ret));

	/* destory enviroments */
	if (good) destory_notify_buf();
}

void *hfuse_ll_notify_loop(void *ptr)
{
	FUSE_NOTIFY_DATA *data;
	int32_t func_num;

	UNUSED(ptr);
	write_log(4, "Debug %s: start\n", __func__);

	while (TRUE) {
		sem_wait(&notify_buf.tasks_sem);
		if (hcfs_system->system_going_down == TRUE)
			break;
		data = notify_buf_dequeue();
		if (data != NULL) {
			func_num = ((FUSE_NOTIFY_PROTO *)data)->func;
			notify_fn[func_num](&data, RUN);
			write_log(10, "Debug %s: notified\n", __func__);
		} else {
			write_log(1, "Debug %s: error\n", __func__);
		}
	}

	write_log(1, "Debug %s: loop end\n", __func__);
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
		write_log(10, "Debug %s: free notify data\n", __func__);
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
		write_log(10, "Debug %s: called\n", __func__);
	}

	/* free nested member */
	free(data->name);
	data->name = NULL;
	/* struct fuse_chan is shared, should not be freed here */

	/* If it's not recycling notify_buf, ptr is copied in
	 * notify_buf_dequeue. It need to be destroyed. */
	if (action != DESTROY_BUF) {
		free(*data_ptr);
		*data_ptr = NULL;
		write_log(10, "Debug %s: free notify data\n", __func__);
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
 */
void hfuse_ll_notify_delete(struct fuse_chan *ch,
			    fuse_ino_t parent,
			    fuse_ino_t child,
			    const char *name,
			    size_t namelen)
{
	BOOL good = TRUE;
	FUSE_NOTIFY_DELETE_DATA event = {.func = DELETE,
					 .ch = ch,
					 .parent = parent,
					 .child = child,
					 .name = NULL,
					 .namelen = namelen};
	good = (event.name = strndup(name, namelen)) != NULL;

	if (good)
		good = (notify_buf_enqueue((void *)&event) == 0);

	if (good)
		/* wake notify thread up */
		good = (sem_post(&notify_buf.tasks_sem) == 0);

	if (good) {
		write_log(4, "Debug %s: Notify queued\n", __func__);
	} else {
		write_log(4, "Debug %s: Failed\n", __func__);
		if (event.name != NULL)
			free(event.name);
	}
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
 */
void hfuse_ll_notify_delete_mp(struct fuse_chan *ch,
			       fuse_ino_t parent,
			       fuse_ino_t child,
			       const char *name,
			       size_t namelen,
			       const char *selfname)
{
	BOOL diffcase = (strcmp(name, selfname) != 0);

	write_log(4, "Debug %s: called\n", __func__);
	if (mount_global.fuse_default != NULL &&
	    (diffcase || mount_global.fuse_default != ch)) {
		hfuse_ll_notify_delete(mount_global.fuse_default, parent, child,
				       name, namelen);
	}
	if (mount_global.fuse_read != NULL &&
	    (diffcase || mount_global.fuse_read != ch)) {
		hfuse_ll_notify_delete(mount_global.fuse_read, parent, child,
				       name, namelen);
	}
	if (mount_global.fuse_write != NULL &&
	    (diffcase || mount_global.fuse_write != ch)) {
		hfuse_ll_notify_delete(mount_global.fuse_write, parent, child,
				       name, namelen);
	}
}
/* END -- Functions running in fuse operation */
