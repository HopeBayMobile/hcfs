#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "fuse_notify.h"
#include "mount_manager.h"
#include "hfuse_system.h"

FUSE_NOTIFY_CYCLE_BUF notify_cb = {0};
#define CB_OFFSET(index) (notify_cb.elems + index * notify_cb.elemsize)

int32_t init_notify_cb(void)
{
	/* May change to largest arg size when using miltiple types */
	notify_cb.elemsize = sizeof(FUSE_NOTIFY_DELETE_DATA);
	notify_cb.max_len = FUSE_NOTIFY_CB_DEFAULT_LEN;
	notify_cb.elems =
	    (void *)malloc(notify_cb.max_len * notify_cb.elemsize);
	sem_init(&notify_cb.tasks_sem, 1, 0);
	sem_init(&notify_cb.access_sem, 1, 1);

	return (notify_cb.elems != NULL) ? 0 : -1;
}

void destory_notify_cb(void)
{
	size_t i, len;
	FUSE_NOTIFY_PROTO *data;

	/* call _do_hfuse_ll_notify_XXX to free nested member */
	for (i = notify_cb.out, len = 1; len <= notify_cb.len; i++, len++) {
		if (i == notify_cb.max_len)
			i = 0;
		data = (FUSE_NOTIFY_PROTO *)CB_OFFSET(i);
		data->func(&data, DESTROY_CB);
	}
	/* free notify_cb member */
	free(notify_cb.elems);
	notify_cb.elems = NULL;

	/* Reset notify_cb in case it's accessed again after been freed */
	memset(&notify_cb, 0, sizeof(FUSE_NOTIFY_CYCLE_BUF));
}

BOOL notify_cb_realloc(void)
{
	void *newbuf = NULL;
	size_t newsize;
	BOOL ok = FALSE;

	if (notify_cb.max_len == 0) {
		init_notify_cb();
		return EXIT_SUCCESS;
	}

	/* enlarge buffer */
	newsize = notify_cb.max_len * 2;
	newbuf = realloc(notify_cb.elems, newsize * notify_cb.elemsize);
	ok = (newbuf != NULL);

	if (ok) {
		notify_cb.elems = newbuf;
		/* Concatenate wrapped segments after enlarge buffer
		 * +------------------------+
		 * |---->|    |<-data-------|
		 * +------------------------+-----------------------+
		 * |          |<-data----------->|                  |
		 * +------------------------------------------------+
		 *       ^    ^                  ^
		 *       |    |                  |
		 *       in   out                in after enlarge buf
		 */
		if (notify_cb.in < notify_cb.out) {
			memcpy(CB_OFFSET(notify_cb.max_len), notify_cb.elems,
			       (notify_cb.in + 1) * notify_cb.elemsize);
			notify_cb.in += notify_cb.max_len;
		}

		notify_cb.max_len = newsize;
		return EXIT_SUCCESS;
	}

	/* !ok */
	write_log(4, "Error %s failed\n", __func__);
	return EXIT_FAILURE;
}

static inline BOOL notify_cb_isfull(void)
{
	return (notify_cb.len == notify_cb.max_len);
}

static inline BOOL notify_cb_isempty(void) { return (notify_cb.len == 0); }

void notify_cb_enqueue(const void *const notify, size_t size)
{
	size_t next;
	BOOL ok = TRUE;

	sem_wait(&notify_cb.access_sem);

	if (notify_cb_isfull())
		ok = (notify_cb_realloc() == EXIT_SUCCESS);

	if (ok) {
		next = notify_cb.in + 1;
		if (next == notify_cb.max_len)
			next = 0;
		memcpy(CB_OFFSET(next), notify, size);
		notify_cb.in = next;
		notify_cb.len++;
	}

	sem_post(&notify_cb.access_sem);
}

/* Dequeue the notify cycle buffer, return NULL with errno if no data or
 * other error happened
 *
 * @return Pointer to FUSE_NOTIFY_PROTO on success, NULL on fail with
 * errno been set.
 */
FUSE_NOTIFY_PROTO *notify_cb_dequeue()
{
	void *data = NULL;
	size_t data_size;
	BOOL ok = FALSE;

	sem_wait(&notify_cb.access_sem);

	if (notify_cb_isempty()) {
		errno = ENODATA;
		return NULL;
	}

	data_size = ((FUSE_NOTIFY_PROTO *)CB_OFFSET(notify_cb.out))->data_size;
	ok = ((data = malloc(data_size)) != NULL);

	if (ok) {
		memcpy(data, CB_OFFSET(notify_cb.out), data_size);
		notify_cb.out++;
		if (notify_cb.out == notify_cb.max_len)
			notify_cb.out = 0;
		notify_cb.len--;
	}

	sem_post(&notify_cb.access_sem);

	if (!ok)
		write_log(4, "Error %s: failed\n", __func__);

	return data;
}

int32_t init_hfuse_ll_notify_loop(void)
{
	pthread_attr_t ptattr;
	BOOL good = TRUE;
	int32_t error = 0;

	/* init enviroments */
	if (notify_cb.max_len == 0)
		good = (0 == init_notify_cb());

	if (good) {
		pthread_attr_init(&ptattr);
		pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_DETACHED);
		error = pthread_create(&fuse_nofify_thread, &ptattr,
				       hfuse_ll_notify_loop, NULL);
		good = (0 == error);
	}

	if (good) {
		write_log(1, "%s: done.\n", __func__);
	}

	return good ? 0 : -error;
}

void destory_hfuse_ll_notify_loop(void)
{
	/* let loop handle destory tasks itself */
	sem_post(&notify_cb.tasks_sem);

	pthread_join(event_loop_thread, NULL);

	/* destory enviroments */
	destory_notify_cb();
}

void *hfuse_ll_notify_loop(void *ptr)
{
	FUSE_NOTIFY_PROTO *data;

	UNUSED(ptr);
	write_log(4, "%s: start.\n", __func__);

	while (TRUE) {
		sem_wait(&notify_cb.tasks_sem);
		if (hcfs_system->system_going_down == TRUE)
			break;
		data = notify_cb_dequeue();
		if (data != NULL)
			data->func(&data, RUN);
	}

	write_log(1, "%s: end.\n", __func__);
	return NULL;
}

/* START -- Functions running in notify thread */
void _do_hfuse_ll_notify_delete(FUSE_NOTIFY_PROTO **data_ptr,
				enum NOTIFY_ACTION action)
{
	FUSE_NOTIFY_DELETE_DATA *data = *((FUSE_NOTIFY_DELETE_DATA **)data_ptr);

	if (action == RUN) {
		fuse_lowlevel_notify_delete(data->ch, data->parent, data->child,
					    data->name, data->namelen);
	}

	/* free nested member */
	free(data->name);
	data->name = NULL;

	/* If it's not recycling notify_cb, ptr is copied in
	 * notify_cb_dequeue. It need to be destroyed. */
	if (action != DESTROY_CB) {
		free(data);
		*data_ptr = NULL;
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
	FUSE_NOTIFY_DELETE_DATA event = {
	    .proto.func = _do_hfuse_ll_notify_delete,
	    .proto.data_size = sizeof(FUSE_NOTIFY_DELETE_DATA),
	    .ch = ch,
	    .parent = parent,
	    .child = child,
	    .name = strdup(name),
	    .namelen = namelen};

	notify_cb_enqueue((void *)&event, sizeof(FUSE_NOTIFY_DELETE_DATA));

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
