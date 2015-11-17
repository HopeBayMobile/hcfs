#include "api_interface_unittest.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "fuseop.h"
#include "global.h"

int write_log(int level, char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

int unmount_all(void)
{
	UNMOUNTEDALL = TRUE;
	return 0;
}

int add_filesystem(char *fsname, DIR_ENTRY *ret_entry)
{
	CREATEDFS = TRUE;
	strcpy(recvFSname, fsname);
	return 0;
}
int delete_filesystem(char *fsname)
{
	strcpy(recvFSname, fsname);
	DELETEDFS = TRUE;
	return 0;
}
int check_filesystem(char *fsname, DIR_ENTRY *ret_entry)
{
	strcpy(recvFSname, fsname);
	CHECKEDFS = TRUE;
	return 0;
}
int list_filesystem(unsigned long buf_num, DIR_ENTRY *ret_entry,
		unsigned long *ret_num)
{
	LISTEDFS = TRUE;
	if (numlistedFS == 0) {
		*ret_num = 0;
	} else {
		*ret_num = 1;
		if (buf_num > 0)
			snprintf(ret_entry[0].d_name, 10, "test123");
	}
	return 0;
}

int mount_FS(char *fsname, char *mp)
{
	MOUNTEDFS = TRUE;
	strcpy(recvFSname, fsname);
	strcpy(recvmpname, mp);
	return 0;
}
int unmount_FS(char *fsname)
{
	UNMOUNTEDFS = TRUE;
	strcpy(recvFSname, fsname);
	return 0;
}
int mount_status(char *fsname)
{
	CHECKEDMOUNT = TRUE;
	strcpy(recvFSname, fsname);
	return 0;
}

int pin_inode(ino_t this_inode, long long *reserved_pinned_size)
{
	if (PIN_INODE_ROLLBACK == TRUE)
		return -EIO;
	return 0;
}

int unpin_inode(ino_t this_inode, long long *reserved_release_size)
{
	return 0;
}
