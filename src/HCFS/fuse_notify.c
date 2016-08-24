#include "fuse_notify.h"
#include "mount_manager.h"
#include <errno.h>
#include <string.h>

FUSE_NOTIFY_CYCLE_BUF notify_cb = {0};
#define CB_OFFSET(index) (notify_cb.elems + index * notify_cb.elemsize)

void notify_cb_init(void)
{
	/* May change to largest arg size when using miltiple types */
	notify_cb.elemsize = sizeof(FUSE_NOTIFY_DELETE_DATA);
	notify_cb.max_len = FUSE_NOTIFY_CB_DEFAULT_LEN;
	notify_cb.elems =
	    (void *)malloc(notify_cb.max_len * notify_cb.elemsize);
	sem_init(&notify_cb.tasks_sem, 1, 0);
}

void notify_cb_destory(void)
{
	free(notify_cb.elems);
	notify_cb.elems = NULL;
	memset(&notify_cb, 0, sizeof(FUSE_NOTIFY_CYCLE_BUF));
}

void notify_cb_realloc(void)
{
	void *newbuf = NULL;
	size_t newsize;
	BOOL ok = FALSE;

	if (notify_cb.max_len == 0) {
		notify_cb_init();
		return;
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
			memcpy(CB_OFFSET(notify_cb.max_len),
			       notify_cb.elems,
			       (notify_cb.in + 1) * notify_cb.elemsize);
			notify_cb.in += notify_cb.max_len;
		}

		notify_cb.max_len = newsize;
	}

	if (!ok)
		write_log(1, "Error %s failed\n", __func__);
}

static inline BOOL notify_cb_isfull()
{
	return notify_cb.len == notify_cb.max_len;
}

static inline BOOL notify_cb_isempty() { return notify_cb.len == 0; }

void notify_cb_enqueue(const void const *notify, size_t size)
{
	size_t next;

	if (notify_cb_isfull())
		notify_cb_realloc();

	next = notify_cb.in + 1;
	if (next == notify_cb.max_len)
		next = 0;

	memcpy(CB_OFFSET(next), notify, size);
	notify_cb.in = next;
	notify_cb.len++;
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

	if (notify_cb_isempty()) {
		errno = ENODATA;
		return NULL;
	}

	data_size = ((FUSE_NOTIFY_PROTO *)CB_OFFSET(notify_cb.out))->data_size;
	data = malloc(data_size);

	if (data != NULL) {
		memcpy(data, CB_OFFSET(notify_cb.out), data_size);
		notify_cb.out++;
		if (notify_cb.out == notify_cb.max_len)
			notify_cb.out = 0;
		notify_cb.len--;
	}
	return data;
}

void hfuse_ll_notify_loop_init(void)
{
	pthread_t tmp_thread;
	pthread_attr_t ptattr;
	pthread_attr_init(&ptattr);
	pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_DETACHED);
	pthread_create(&tmp_thread, &ptattr, hfuse_ll_notify_loop, NULL);
}

void hfuse_ll_notify_loop_destory(void) { sem_post(&notify_cb.tasks_sem); }

void *hfuse_ll_notify_loop(void *ptr)
{

	UNUSED(ptr);

	while (TRUE) {
		sem_wait(&notify_cb.tasks_sem);
		if (hcfs_system->system_going_down == TRUE)
			break;

	}
	return NULL;
}

/* START -- Functions called from fuse operation */
void _do_hfuse_ll_notify_delete(void *data) { UNUSED(data); }
/* END -- Functions running in notify thread */


/* START -- Functions running in fuse operation thread */
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

void hfuse_ll_notify_delete(struct fuse_chan *ch,
			    fuse_ino_t parent,
			    fuse_ino_t child,
			    const char *name,
			    size_t namelen)
{
	FUSE_NOTIFY_DELETE_DATA event = {.func = _do_hfuse_ll_notify_delete,
					 .data_size =
					     sizeof(FUSE_NOTIFY_DELETE_DATA),
					 .ch = ch,
					 .parent = parent,
					 .child = child,
					 .name = strdup(name),
					 .namelen = namelen};

	sem_wait(&(hcfs_system->fuse_nofify_sem));
	notify_cb_enqueue((void *)&event, sizeof(FUSE_NOTIFY_DELETE_DATA));
	sem_post(&(hcfs_system->fuse_nofify_sem));

	/* wake notify thread up */
	sem_post(&notify_cb.tasks_sem);

	return;
}
/* END -- Functions running in fuse operation thread */
