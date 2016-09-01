#include "fuse_notify.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "hfuse_system.h"
#include "mount_manager.h"

FUSE_NOTIFY_CYCLE_BUF notify_cb = {0};
fuse_notify_fn *notify_fn[] = {_do_hfuse_ll_notify_noop,
			       _do_hfuse_ll_notify_delete};

/* Helper functions */

#define check_cb_elem_size(X)                                                  \
	_Static_assert(FUSE_NOTIFY_CB_ELEMSIZE >= sizeof(X),                   \
		       "FUSE_NOTIFY_CB_ELEMSIZE is not enough for " #X)

static inline BOOL notify_cb_isempty() { return notify_cb.len == 0; }
static inline BOOL notify_cb_isfull()
{
	return notify_cb.len == notify_cb.max_len;
}
static inline void inc_cb_idx(size_t *x, size_t cb_size)
{
	(*x)++;
	if ((*x) == cb_size)
		(*x) = 0;
}

int32_t init_notify_cb(void)
{
	BOOL good = TRUE;
	errno = 0;

	check_cb_elem_size(FUSE_NOTIFY_DELETE_DATA);

	notify_cb.max_len = FUSE_NOTIFY_CB_DEFAULT_LEN;
	notify_cb.in = notify_cb.max_len - 1;

	notify_cb.elems =
	    (void *)malloc(notify_cb.max_len * sizeof(FUSE_NOTIFY_DATA));
	good = (notify_cb.elems != NULL);

	if (good)
		good = (0 == sem_init(&notify_cb.tasks_sem, 1, 0));
	if (good)
		good = (0 == sem_init(&notify_cb.access_sem, 1, 1));

	if (good) {
		notify_cb.is_initialized = TRUE;
		write_log(4, "Debug %s: succeed. max %lu\n", __func__,
			  notify_cb.max_len);
	} else {
		notify_cb.is_initialized = FALSE;
		free(notify_cb.elems);
		notify_cb.elems = NULL;
		write_log(4, "Debug %s: failed. %s\n", __func__,
			  strerror(errno));
	}
	return good ? 0 : -1;
}

void destory_notify_cb(void)
{
	size_t len;
	FUSE_NOTIFY_PROTO *data;

	if (notify_cb.is_initialized == FALSE)
		return;

	sem_wait(&notify_cb.access_sem);

	/* call _do_hfuse_ll_notify_XXX to free nested member */
	for (len = 1; len <= notify_cb.len; len++) {

		data = (FUSE_NOTIFY_PROTO *)&notify_cb.elems[notify_cb.out];
		/* call destroy action of each data */
		notify_fn[data->func]((FUSE_NOTIFY_DATA **)&data, DESTROY_CB);
		inc_cb_idx(&notify_cb.out, notify_cb.max_len);
	}
	/* free notify_cb member */
	free(notify_cb.elems);
	notify_cb.elems = NULL;

	notify_cb.is_initialized = FALSE;

	sem_post(&notify_cb.access_sem);

	write_log(4, "Debug %s: freed notify_cb.elems\n", __func__);
}

BOOL notify_cb_realloc(void)
{
	void *newbuf = NULL;
	size_t newsize;
	BOOL good = notify_cb.is_initialized;

	/* enlarge buffer */
	if (good) {
		newsize = notify_cb.max_len * 2;
		newbuf = realloc(notify_cb.elems,
				 newsize * sizeof(FUSE_NOTIFY_DATA));
		good = (newbuf != NULL);
	}

	if (good) {
		notify_cb.elems = newbuf;

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
		 */
		if (notify_cb.in < notify_cb.out) {
			memcpy(&notify_cb.elems[notify_cb.max_len],
			       notify_cb.elems,
			       (notify_cb.in + 1) * sizeof(FUSE_NOTIFY_DATA));
			notify_cb.in += notify_cb.max_len;
		}
		notify_cb.max_len = newsize;
		write_log(4, "Debug %s: done. resize to %lu\n", __func__,
			  notify_cb.max_len);
	} else {
		write_log(4, "Debug %s: failed\n", __func__);
	}

	return good ? EXIT_SUCCESS : EXIT_FAILURE;
}

void notify_cb_enqueue(const void *const notify)
{
	BOOL good = notify_cb.is_initialized;

	if (!good) {
		write_log(4, "Debug %s: notify buffer is not initialized\n",
			  __func__);
		return;
	}

	sem_wait(&notify_cb.access_sem);

	if (notify_cb_isfull()) {
		write_log(4, "Debug %s: queue is full with %lu notify\n",
			  __func__, notify_cb.len);
		good = (notify_cb_realloc() == EXIT_SUCCESS);
	}

	if (good) {
		inc_cb_idx(&notify_cb.in, notify_cb.max_len);
		memcpy(&notify_cb.elems[notify_cb.in], notify,
		       sizeof(FUSE_NOTIFY_DATA));
		notify_cb.len++;
	}

	sem_post(&notify_cb.access_sem);

	if (good)
		write_log(4, "Debug %s: Add %lu\n", __func__, notify_cb.in);
	else
		write_log(4, "Debug %s: failed\n", __func__);
}

/* Dequeue the notify cycle buffer, return NULL with errno if no data or
 * other ret_val happened
 *
 * @return Pointer to FUSE_NOTIFY_PROTO on success, NULL on fail with
 * errno been set.
 */
FUSE_NOTIFY_DATA *notify_cb_dequeue()
{
	FUSE_NOTIFY_DATA *data = NULL;
	BOOL cb_isempty;
	BOOL good = notify_cb.is_initialized;

	sem_wait(&notify_cb.access_sem);

	if(good)
		good = ((cb_isempty = notify_cb_isempty()) == 0);

	if (good)
		good = ((data = malloc(sizeof(FUSE_NOTIFY_DATA))) != NULL);

	if (good) {
		memcpy(data, &notify_cb.elems[notify_cb.out],
		       sizeof(FUSE_NOTIFY_DATA));
		inc_cb_idx(&notify_cb.out, notify_cb.max_len);
		notify_cb.len--;
	}

	sem_post(&notify_cb.access_sem);

	if (good)
		write_log(4, "Debug %s: Get %lu\n", __func__, notify_cb.out);
	else if (notify_cb.is_initialized == 0)
		write_log(4, "Debug %s: failed. %s\n", __func__,
			  "Buffer is not initialized.");
	else if (cb_isempty)
		write_log(4, "Debug %s: failed. %s\n", __func__,
			  "Trying to dequeue an empty queue.");
	else
		write_log(4, "Debug %s: failed. %s\n", __func__,
			  "malloc failed.");

	return data;
}

int32_t init_hfuse_ll_notify_loop(void)
{
	BOOL good = TRUE;
	int32_t ret_val = 0;

	/* init enviroments */
	if (notify_cb.is_initialized == FALSE) {
		ret_val = init_notify_cb();
		good = (0 == ret_val);
	}

	if (good) {
		ret_val = -pthread_create(&fuse_nofify_thread, NULL,
					  hfuse_ll_notify_loop, NULL);
		good = (0 == ret_val);
	}

	if (good)
		write_log(10, "Debug %s succeed\n", __func__);
	else
		write_log(1, "%s failed\n", __func__);

	return ret_val;
}

void destory_hfuse_ll_notify_loop(void)
{
	BOOL good = TRUE;
	int32_t ret = 0;
	/* let loop handle destory tasks itself */
	sem_post(&notify_cb.tasks_sem);

	good = (ret = pthread_join(fuse_nofify_thread, NULL)) == 0;
	if (!good)
		write_log(1, "Error %s: join nofify_thread failed. %s\n",
			  __func__, strerror(ret));

	/* destory enviroments */
	if (good)
		destory_notify_cb();
}

void *hfuse_ll_notify_loop(void *ptr)
{
	FUSE_NOTIFY_DATA *data;
	int32_t func_num;

	UNUSED(ptr);
	write_log(4, "Debug %s: start\n", __func__);

	while (TRUE) {
		sem_wait(&notify_cb.tasks_sem);
		if (hcfs_system->system_going_down == TRUE)
			break;
		data = notify_cb_dequeue();
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
void _do_hfuse_ll_notify_noop(__attribute__((unused)) FUSE_NOTIFY_DATA **a,
			      __attribute__((unused)) enum NOTIFY_ACTION b)
{
}

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

	/* If it's not recycling notify_cb, ptr is copied in
	 * notify_cb_dequeue. It need to be destroyed. */
	if (action != DESTROY_CB) {
		free(*data_ptr);
		*data_ptr = NULL;
		write_log(10, "Debug %s: free notify data\n", __func__);
	}
}
/* END -- Functions running in notify thread */

/* START -- Functions running in fuse operation */
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
					 .name = strdup(name),
					 .namelen = namelen};
	good = (event.name = strndup(name, namelen)) != NULL;
	if (good) {
		notify_cb_enqueue((void *)&event);
		write_log(4, "Debug %s: notify queued\n", __func__);
	} else {
		write_log(4, "Debug %s: failed, strndup error\n", __func__);
	}

	/* wake notify thread up */
	sem_post(&notify_cb.tasks_sem);
}

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
