#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <inttypes.h>

#include "fuseop.h"
#include "global.h"

#define METAPATH "/data/hcfs/metastorage"
#define BLOCKPATH "/data/hcfs/blockstorage"
#define RESTORE_METAPATH "/data/hcfs/metastorage_restore"
#define RESTORE_BLOCKPATH "/data/hcfs/blockstorage_restore"

/************************************************************************
*
* Function name: fetch_meta_path
*        Inputs: char *pathname, ino_t this_inode
*        Output: Integer
*       Summary: Given the inode number this_inode,
*                copy the filename to the meta file to the space pointed
*                by pathname.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t fetch_meta_path(char *pathname, ino_t this_inode)
{
	char tempname[METAPATHLEN];
	int32_t sub_dir;

	if (METAPATH == NULL)
		return -1;

	/* Creates meta path if it does not exist */
	if (access(METAPATH, F_OK) == -1)
		mkdir(METAPATH, 0700);

	sub_dir = this_inode % NUMSUBDIR;
	snprintf(tempname, METAPATHLEN, "%s/sub_%d", METAPATH, sub_dir);

	/* Creates meta path for meta subfolder if it does not exist */
	if (access(tempname, F_OK) == -1)
		mkdir(tempname, 0700);

	snprintf(pathname, METAPATHLEN, "%s/sub_%d/meta%" PRIu64 "",
		METAPATH, sub_dir, (uint64_t)this_inode);

	return 0;
}

int32_t fetch_restore_meta_path(char *pathname, ino_t this_inode)
{
	char tempname[METAPATHLEN];
	int32_t sub_dir;

	if (RESTORE_METAPATH == NULL)
		return -1;

	/* Creates meta path if it does not exist */
	if (access(RESTORE_METAPATH, F_OK) == -1)
		mkdir(RESTORE_METAPATH, 0700);

	sub_dir = this_inode % NUMSUBDIR;
	snprintf(tempname, METAPATHLEN, "%s/sub_%d", RESTORE_METAPATH, sub_dir);

	/* Creates meta path for meta subfolder if it does not exist */
	if (access(tempname, F_OK) == -1)
		mkdir(tempname, 0700);

	snprintf(pathname, METAPATHLEN, "%s/sub_%d/meta%" PRIu64 "",
		RESTORE_METAPATH, sub_dir, (uint64_t)this_inode);

	return 0;
}

/************************************************************************
*
* Function name: fetch_block_path
*        Inputs: char *pathname, ino_t this_inode, int64_t block_num
*       Summary: Given the inode number this_inode,
*                copy the path to the block "block_num" to "pathname".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t fetch_block_path(char *pathname, ino_t this_inode, int64_t block_num)
{
	char tempname[BLOCKPATHLEN];
	int32_t sub_dir;
	int32_t errcode, ret;

	if (BLOCKPATH == NULL)
		return -EPERM;

	if (access(BLOCKPATH, F_OK) == -1)
		mkdir(BLOCKPATH, 0700);

	sub_dir = (this_inode + block_num) % NUMSUBDIR;
	snprintf(tempname, BLOCKPATHLEN, "%s/sub_%d", BLOCKPATH, sub_dir);
	if (access(tempname, F_OK) == -1)
		mkdir(tempname, 0700);

	snprintf(pathname, BLOCKPATHLEN, "%s/sub_%d/block%" PRIu64 "_%"PRId64,
			BLOCKPATH, sub_dir, (uint64_t)this_inode, block_num);

	return 0;

errcode_handle:
	return errcode;
}

int32_t fetch_restore_block_path(char *pathname, ino_t this_inode, int64_t block_num)
{
	char tempname[BLOCKPATHLEN];
	int32_t sub_dir;
	int32_t errcode, ret;

	if (RESTORE_BLOCKPATH == NULL)
		return -EPERM;

	if (access(RESTORE_BLOCKPATH, F_OK) == -1)
		mkdir(RESTORE_BLOCKPATH, 0700);

	sub_dir = (this_inode + block_num) % NUMSUBDIR;
	snprintf(tempname, BLOCKPATHLEN, "%s/sub_%d", RESTORE_BLOCKPATH, sub_dir);
	if (access(tempname, F_OK) == -1)
		mkdir(tempname, 0700);

	snprintf(pathname, BLOCKPATHLEN, "%s/sub_%d/block%" PRIu64 "_%"PRId64,
			RESTORE_BLOCKPATH, sub_dir, (uint64_t)this_inode, block_num);

	return 0;

errcode_handle:
	return errcode;
}

void _copyfile(char *srcpath, char *despath)
{
	FILE *fptr1, *fptr2;
	size_t retsize;
	unsigned char buf[4096];

	fptr1 = fopen(srcpath, "r");
	if (fptr1 == NULL)
		return;
	fptr2 = fopen(despath, "w");
	if (fptr2 == NULL) {
		fclose(fptr1);
		return;
	}

	flock(fileno(fptr1), LOCK_EX);
	while (!feof(fptr1)) {
		retsize = fread(buf, 1, 4096, fptr1);
		if (retsize == 0)
			break;
		fwrite(buf, 1, retsize, fptr2);
	}
	flock(fileno(fptr1), LOCK_UN);
	fclose(fptr1);
	fclose(fptr2);
}

void _copy_meta(ino_t thisinode)
{
	char srcpath[METAPATHLEN];
	char despath[METAPATHLEN];

	fetch_meta_path(srcpath, thisinode);
	fetch_restore_meta_path(despath, thisinode);

	_copyfile(srcpath, despath);
}

void _copy_block(ino_t thisinode, int64_t blockno)
{
	char srcpath[METAPATHLEN];
	char despath[METAPATHLEN];

	fetch_block_path(srcpath, thisinode, blockno);
	fetch_restore_block_path(despath, thisinode, blockno);

	_copyfile(srcpath, despath);
}

void _copy_pinned(ino_t thisinode)
{
	FILE_META_TYPE tmpmeta;
	struct stat tmpstat;
	FILE *fptr;
	char metapath[METAPATHLEN];
	int64_t count, totalblocks, tmpsize;

	fetch_restore_meta_path(metapath, thisinode);
	fptr = fopen(metapath, "r");
	fread(&tmpstat, sizeof(struct stat), 1, fptr);
	fread(&tmpmeta, sizeof(FILE_META_TYPE), 1, fptr);
	if (tmpmeta.local_pin == P_UNPIN) {
		/* Don't copy */
		fclose(fptr);
		return;
	}

	/* Assume fixed block size now */
	tmpsize = tmpstat.st_size;
	totalblocks = ((tmpsize - 1) / 1048576) + 1;
	for (count = 0; count < totalblocks; count++)
		_copy_block(thisinode, count);

	fclose(fptr);
}

void _expand_and_copy(ino_t thisinode, char force_expand)
{
	FILE *fptr1;
	char copiedmeta[METAPATHLEN];
	DIR_META_TYPE dirmeta;
	DIR_ENTRY_PAGE tmppage;
	int64_t filepos;
	int32_t count;
	ino_t tmpino;
	DIR_ENTRY *tmpptr;

	fetch_restore_meta_path(copiedmeta, thisinode);
	fptr1 = fopen(copiedmeta, "r");

	fseek(fptr1, sizeof(struct stat), SEEK_SET);
	fread(&dirmeta, sizeof(DIR_META_TYPE), 1, fptr1);

	/* Do not expand if not high priority pin and not needed */
	if ((dirmeta.local_pin != P_HIGH_PRI_PIN) && (force_expand == FALSE))
		return;

	/* Fetch first page */
	filepos = dirmeta.tree_walk_list_head;

	while (filepos != 0) {
		fseek(fptr1, filepos, SEEK_SET);
		fread(&tmppage, sizeof(DIR_ENTRY_PAGE), 1, fptr1);
		for (count = 0; count < tmppage.num_entries; count++) {
			tmpptr = &(tmppage.dir_entries[count]);
			
			if (tmpptr->d_ino == 0)
				continue;
			/* Skip "." and ".." */
			if (strcmp(tmpptr->d_name, ".") == 0)
				continue;
			if (strcmp(tmpptr->d_name, "..") == 0)
				continue;

			/* First copy the meta */
			tmpino = tmpptr->d_ino;
			_copy_meta(tmpino);
			switch (tmpptr->d_type) {
			case D_ISLNK:
				/* Just copy the meta */
				break;
			case D_ISREG:
			case D_ISFIFO:
			case D_ISSOCK:
				/* Copy all blocks if pinned */
				_copy_pinned(tmpino);
				break;
			case D_ISDIR:
				/* Need to expand */
				/* TODO: for some specific path, may need
				to force expansion */
				_expand_and_copy(tmpino, FALSE);
				break;
			default:
				break;
			}
		}
		/* Continue to the next page */
		filepos = tmppage.tree_walk_next;
	}
	fclose(fptr1);
}

int32_t main(void)
{
	struct stat tmpstat;
	ino_t rootino;

	/* TODO: Need to copy the sys and stat meta */
	/* First check /data/app */

	stat("/data/app", &tmpstat);
	rootino = tmpstat.st_ino;

	_copy_meta(rootino);

	_expand_and_copy(rootino, TRUE);

	return 0;
}
