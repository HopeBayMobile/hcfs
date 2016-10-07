#include <fuse/fuse_lowlevel.h>

int32_t hfuse_ll_notify_delete(struct fuse_chan *ch,
			    fuse_ino_t parent,
			    fuse_ino_t child,
			    const char *name,
			    size_t namelen)
{
}
void hfuse_ll_notify_delete_mp(struct fuse_chan *ch,
			       fuse_ino_t parent,
			       fuse_ino_t child,
			       const char *name,
			       size_t namelen,
			       const char *selfname)
{
}
int32_t init_hfuse_ll_notify_loop(void) { return 0; }
void destory_hfuse_ll_notify_loop(void) {}
