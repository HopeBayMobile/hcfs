#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "fuseop.h"
#include "global.h"
#include "hcfscurl.h"
#include "mount_manager_unittest.h"
#include "lookup_count.h"

struct fuse_chan {
	int test
} tmpchan;
extern SYSTEM_CONF_STRUCT system_config;

struct fuse_lowlevel_ops {
	int test;
} hfuse_ops;

struct fuse_session {
	int test;
} tmpsession;

int write_log(int level, char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

struct fuse_chan* fuse_mount(char *mp, struct fuse_args *args)
{
	return &tmpchan;
}

struct fuse_session* fuse_lowlevel_new(struct fuse_args *args,
	struct fuse_lowlevel_ops *fuse_ops, size_t size, void *ptr)
{
	return &tmpsession;
}

void fuse_set_signal_handlers(struct fuse_session *ptr)
{
	return;
}

void fuse_session_add_chan(struct fuse_session *ptr, struct fuse_chan *ptr2)
{
	return;
}

void* mount_multi_thread(void *ptr)
{
	return NULL;
}

void* mount_single_thread(void *ptr)
{
	return NULL;
}

void fuse_remove_signal_handlers(struct fuse_session *ptr)
{
	return;
}

void fuse_session_remove_chan(struct fuse_chan *ptr2)
{
	return;
}

void fuse_session_destroy(struct fuse_session *ptr)
{
	return;
}

void fuse_parse_cmdline(struct fuse_args *ptr, char **mp, int *mt, int *fg)
{
	return;
}
void fuse_opt_free_args(struct fuse_args *ptr)
{
	return;
}
void fuse_unmount(char *mp, struct fuse_chan *ptr2)
{
	return;
}

int check_filesystem_core(char *fsname, DIR_ENTRY *ret_entry)
{
	ret_entry->d_ino = 100;
	return FS_CORE_FAILED;
}

int lookup_init(LOOKUP_HEAD_TYPE *lookup_table)
{
	return 0;
}

