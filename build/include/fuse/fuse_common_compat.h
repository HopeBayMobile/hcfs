/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB.

  File modified @ Hope Bay Technologies, Inc. (2016)

  1/15/16: Jiahong modified fuse_mount_compat25 to include premount
*/

/* these definitions provide source compatibility to prior versions.
   Do not include this file directly! */

struct fuse_file_info_compat {
	int flags;
	unsigned long fh;
	int writepage;
	unsigned int direct_io : 1;
	unsigned int keep_cache : 1;
};

/* 1/15/16: Jiahong modified fuse_mount_compat25 to include premount */
int fuse_premount_compat25(const char *mountpoint, struct fuse_args *args);
int fuse_mount_compat25(const char *mountpoint, struct fuse_args *args, int fd);

int fuse_mount_compat22(const char *mountpoint, const char *opts);

int fuse_mount_compat1(const char *mountpoint, const char *args[]);

void fuse_unmount_compat22(const char *mountpoint);
