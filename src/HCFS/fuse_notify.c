#include "fuse_notify.h"

NOTIFY_CYCLE_BUF notify_cb={0};

void notify_cb_init(void)
{
	notify_cb.notifies = (FUSE_LL_NOTIFY *)malloc(
	    FUSE_NOTIFY_QUEUE_DEFAULT_LEN * sizeof(FUSE_LL_NOTIFY));
	notify_cb.size = FUSE_NOTIFY_QUEUE_DEFAULT_LEN;
	notify_cb.start = 0;
	notify_cb.end = 0;
}

void notify_cb_realloc(void)
{
	FUSE_LL_NOTIFY *newarr = NULL;
	int32_t newsize = notify_cb.size * 2;
	BOOL ok = FALSE;

	newarr = (FUSE_LL_NOTIFY *)realloc(
	    notify_cb.notifies, notify_cb.size * sizeof(FUSE_LL_NOTIFY));
	ok = newarr != NULL;

	if(ok) {
		notify_cb.notifies = newarr;
		/* if queue is cut into 2 parts, concate them in larger space */
		if (end >= notify_cb.size)
			memcpy(&notify_cb.notifies[notify_cb.size],
			       notify_cb.notifies,
			       (end - notify_cb.size + 1) *
				   sizeof(FUSE_LL_NOTIFY));

		notify_cb.size = newsize;
	}

	if(!ok)
		write_log(1, "Error %s failed\n", __func__);
}

inline BOOL _hfuse_ll_notify_isfull(){
	return (notify_cb.start == notify_cb.end) && notify_cb.size != 0;
}

inline BOOL _hfuse_ll_notify_isempty(){
	return  notify_cb.size == 0;
}

void _hfuse_ll_notify_enqueue(FUSE_LL_NOTIFY *notify)
{
	if(notify_cb.notifies == NULL || )
		_hfuse_ll_notify_realloc();
	if ((fuse_notify_bend + 1) % fuse_notify_blen == fuse_notify_bstart)
	{
	}
		if (task == NULL)
			return;

	if (fuse_global_notify_queue.enqueue != NULL) {
		fuse_global_notify_queue.enqueue->next = task;
		task->next = NULL;
	}
	fuse_global_notify_queue.enqueue = task;

	if(fuse_global_notify_queue.dequeue == NULL){
		fuse_global_notify_queue.dequeue = task;
	}
}

void _hfuse_ll_notify_dequeue(FUSE_LL_NOTIFY *notify) {}

typedef struct {
	MOUNT_T *mount_ptr;
	fuse_ino_t parent;
	fuse_ino_t child;
	const char *name;
} hfuse_ll_notify_delete_args;

void *hfuse_ll_notify_delete(void *ptr)
{
	sem_wait(&(hcfs_system->fuse_nofify_thread_sem));

	if (hcfs_system->fuse_nofify_thread_running == FALSE) {
	}
	sme_post(&(hcfs_system->fuse_nofify_thread_sem));

	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		pthread_t tmp_thread;
		pthread_attr_t ptattr;
		pthread_attr_init(&ptattr);
		pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_DETACHED);
		hfuse_ll_notify_delete_args *args =
		    (hfuse_ll_notify_delete_args *)malloc(
			sizeof(hfuse_ll_notify_delete_args));
		args->mount_ptr = tmpptr;
		args->parent = parent_inode;
		args->child = temp_dentry.d_ino;
		args->name = strndup(temp_dentry.d_name, MAX_FILENAME_LEN);
		hfuse_ll_notify_delete(args);
		if (args->name != NULL) {
			pthread_create(&tmp_thread, &ptattr,
				       hfuse_ll_notify_delete, (void *)args);
		}
	}
#define MSG_NOTIFY_DELETE "fuse_lowlevel_notify_delete"
	hfuse_ll_notify_delete_args args;
	int thread_num;

	memcpy(&args, ptr, sizeof(hfuse_ll_notify_delete_args));
	free(ptr);
	/* Tracing concurrent thread number */
	sem_post(&mount_global.sem);
	sem_getvalue(&mount_global.sem, &thread_num);
	write_log(10, "Debug " MSG_NOTIFY_DELETE " Running %d thread(s)\n",
		  thread_num);
	write_log(10, "Debug " MSG_NOTIFY_DELETE " %s, %" PRIu64 "\n",
		  args.name, (uint64_t)args.child);

	/* notify VFS for following 2 cases:
	 * 1. File is deleted in different latter case
	 * 2. Notify other mount points to trigger kernel to release
	 * lookup on all volumns */
	if (mount_global.fuse_default != NULL) {
		fuse_lowlevel_notify_delete(mount_global.fuse_default, args.parent,
					    args.child, args.name, strlen(args.name));
	}
	if (mount_global.fuse_read != NULL) {
		fuse_lowlevel_notify_delete(mount_global.fuse_read, args.parent,
					    args.child, args.name, strlen(args.name));
	}
	if (mount_global.fuse_write != NULL) {
		fuse_lowlevel_notify_delete(mount_global.fuse_write, args.parent,
					    args.child, args.name, strlen(args.name));
	}
	sem_trywait(&mount_global.sem);
	return NULL;
#undef MSG_NOTIFY_DELETE
}
