/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: utils.c
* Abstract: The c source code file for the utility functions for HCFS
*
* Revision History
* 2015/1/27 Jiahong added header for this file, and comment headers for
*           the functions.
* 2015/1/27 Jiahong revised the coding format for coding style check.
* 2015/2/11 Jiahong revised coding style and add hfuse_system.h inclusion.
*
**************************************************************************/

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <errno.h>
#include <limits.h>

#include "global.h"
#include "fuseop.h"
#include "params.h"
#include "hfuse_system.h"

SYSTEM_CONF_STRUCT system_config;

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
int fetch_meta_path(char *pathname, ino_t this_inode)
{
	char tempname[METAPATHLEN];
	int sub_dir;
	int ret_code = 0;

	if (METAPATH == NULL)
		return -1;

	/* Creates meta path if it does not exist */
	if (access(METAPATH, F_OK) == -1) {
		ret_code = mkdir(METAPATH, 0700);
		if (ret_code < 0)
			return ret_code;
	}

	sub_dir = this_inode % NUMSUBDIR;
	sprintf(tempname, "%s/sub_%d", METAPATH, sub_dir);

	/* Creates meta path for meta subfolder if it does not exist */
	if (access(tempname, F_OK) == -1) {
		ret_code = mkdir(tempname, 0700);

		if (ret_code < 0)
			return ret_code;
	}

	sprintf(tempname, "%s/sub_%d/meta%lld", METAPATH, sub_dir,
		this_inode);
	strcpy(pathname, tempname);

	return 0;
}

/************************************************************************
*
* Function name: fetch_todelete_path
*        Inputs: char *pathname, ino_t this_inode
*        Output: Integer
*       Summary: Given the inode number this_inode,
*                copy the filename to the meta file in todelete folder
*                to the space pointed by pathname.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*          Note: This function is used for post-deletion handling of meta.
*
*************************************************************************/
int fetch_todelete_path(char *pathname, ino_t this_inode)
{
	char tempname[400];
	int sub_dir;
	int ret_code = 0;

	if (METAPATH == NULL)
		return -1;

	if (access(METAPATH, F_OK) == -1) {
		ret_code = mkdir(METAPATH, 0700);
		if (ret_code < 0)
			return ret_code;
	}

	sub_dir = this_inode % NUMSUBDIR;
	sprintf(tempname, "%s/todelete", METAPATH);
	if (access(tempname, F_OK) == -1) {
		ret_code = mkdir(tempname, 0700);

		if (ret_code < 0)
			return ret_code;
	}
	sprintf(tempname, "%s/todelete/sub_%d", METAPATH, sub_dir);
	if (access(tempname, F_OK) == -1) {
		ret_code = mkdir(tempname, 0700);

		if (ret_code < 0)
			return ret_code;
	}
	sprintf(tempname, "%s/todelete/sub_%d/meta%lld", METAPATH, sub_dir,
								this_inode);
	strcpy(pathname, tempname);
	return 0;
}

/************************************************************************
*
* Function name: fetch_block_path
*        Inputs: char *pathname, ino_t this_inode, long long block_num
*       Summary: Given the inode number this_inode,
*                copy the path to the block "block_num" to "pathname".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	char tempname[400];
	int sub_dir;
	int ret_code = 0;

	if (BLOCKPATH == NULL)
		return -1;

	if (access(BLOCKPATH, F_OK) == -1) {
		ret_code = mkdir(BLOCKPATH, 0700);
		if (ret_code < 0)
			return ret_code;
	}

	sub_dir = (this_inode + block_num) % NUMSUBDIR;
	sprintf(tempname, "%s/sub_%d", BLOCKPATH, sub_dir);
	if (access(tempname, F_OK) == -1) {
		ret_code = mkdir(tempname, 0700);

		if (ret_code < 0)
			return ret_code;
	}
	sprintf(tempname, "%s/sub_%d/block%lld_%lld", BLOCKPATH, sub_dir,
						this_inode, block_num);
	strcpy(pathname, tempname);
	return 0;
}

/************************************************************************
*
* Function name: parse_parent_self
*        Inputs: const char *pathname, char *parentname, char *selfname
*       Summary: Given the path "pathname", breaks it down into two components:
*                "selfname" is the name of the object that "pathname" refers
*                to, and "parentname" is the parent of "selfname" in this path.
*  Return value: 0 if successful. Otherwise returns -1.
*
*          Note: Inputs to parse_parent_self need to properly allocate memory.
*
*************************************************************************/
int parse_parent_self(const char *pathname, char *parentname, char *selfname)
{
	int count;

	if (pathname == NULL)
		return -1;

	if (parentname == NULL)
		return -1;

	if (selfname == NULL)
		return -1;

	if (pathname[0] != '/')	 /* Does not handle relative path */
		return -1;

	if (strlen(pathname) <= 1)  /*This is the root, so no parent*/
	 return -1;

	for (count = strlen(pathname)-1; count >= 0; count--) {
		if ((pathname[count] == '/') && (count < (strlen(pathname)-1)))
			break;
	}

	if (count == 0) {
		strcpy(parentname, "/");
		if (pathname[strlen(pathname)-1] == '/') {
			strncpy(selfname, &(pathname[1]), strlen(pathname)-2);
			selfname[strlen(pathname)-2] = 0;
		} else {
			strcpy(selfname, &(pathname[1]));
		}
	} else {
		strncpy(parentname, pathname, count);
		parentname[count] = 0;
		if (pathname[strlen(pathname)-1] == '/') {
			strncpy(selfname, &(pathname[count+1]),
						strlen(pathname)-count-2);
			selfname[strlen(pathname)-count-2] = 0;
		} else {
			strcpy(selfname, &(pathname[count+1]));
		}
	}
	return 0;
}

/************************************************************************
*
* Function name: read_system_config
*        Inputs: char *config_path
*       Summary: Read system configuration from file "config_path", and
*                parse the content.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int read_system_config(char *config_path)
{
	FILE *fptr;
	char tempbuf[200], *ret_ptr, *num_check_ptr;
	char argname[200], argval[200], *tokptr1, *tokptr2, *toktmp, *strptr;
	long long temp_val;

	fptr = fopen(config_path, "r");

	if (fptr == NULL) {
		printf("Cannot open config file (%s) for reading\n",
								config_path);
		return -1;
	}

	while (!feof(fptr)) {
		ret_ptr = fgets(tempbuf, 180, fptr);
		if (ret_ptr == NULL)
			break;

		if (strlen(tempbuf) > 170) {
			printf("Length of option value exceeds limit (170 chars). Exiting.\n");
			return -1;
		}
		if (tempbuf[strlen(tempbuf)-1] == '\n')
			tempbuf[strlen(tempbuf)-1] = 0;

		/*Now decompose the option line into param name and value*/

		toktmp = strtok_r(tempbuf, "=", &tokptr1);

		if (toktmp == NULL)
			continue;

		/*Get rid of the leading and trailing space chars*/

		strptr = toktmp;
		while (*strptr == ' ')
			strptr = strptr + sizeof(char);

		strcpy(argname, strptr);
		while (argname[strlen(argname)-1] == ' ')
			argname[strlen(argname)-1] = 0;

		/*Continue with the param value*/
		toktmp = strtok_r(NULL, "=", &tokptr1);
		if (toktmp == NULL)
			continue;

		strptr = toktmp;
		while (*strptr == ' ')
			strptr = strptr + sizeof(char);
		strcpy(argval, strptr);
		while (argval[strlen(argval)-1] == ' ')
			argval[strlen(argval)-1] = 0;

		/*Match param name with required params*/
		if (strcasecmp(argname, "metapath") == 0) {
			METAPATH = (char *) malloc(strlen(argval) + 10);
			strcpy(METAPATH, argval);
			continue;
		}
		if (strcasecmp(argname, "blockpath") == 0) {
			BLOCKPATH = (char *) malloc(strlen(argval) + 10);
			strcpy(BLOCKPATH, argval);
			continue;
		}
		if (strcasecmp(argname, "superblock") == 0) {
			SUPERBLOCK = (char *) malloc(strlen(argval) + 10);
			strcpy(SUPERBLOCK, argval);
			continue;
		}
		if (strcasecmp(argname, "unclaimedfile") == 0) {
			UNCLAIMEDFILE = (char *) malloc(strlen(argval) + 10);
			strcpy(UNCLAIMEDFILE, argval);
			continue;
		}
		if (strcasecmp(argname, "hcfssystem") == 0) {
			HCFSSYSTEM = (char *) malloc(strlen(argval) + 10);
			strcpy(HCFSSYSTEM, argval);
			continue;
		}
		if (strcasecmp(argname, "cache_soft_limit") == 0) {
			errno = 0;
			temp_val = strtoll(argval, &num_check_ptr, 10);
			if ((errno != 0) || (*num_check_ptr != '\0')) {
				fclose(fptr);
				printf("Number conversion error\n");
				return -1;
			}
			CACHE_SOFT_LIMIT = temp_val;
			continue;
		}
		if (strcasecmp(argname, "cache_hard_limit") == 0) {
			errno = 0;
			temp_val = strtoll(argval, &num_check_ptr, 10);
			if ((errno != 0) || (*num_check_ptr != '\0')) {
				fclose(fptr);
				printf("Number conversion error\n");
				return -1;
			}
			CACHE_HARD_LIMIT = temp_val;
			continue;
		}
		if (strcasecmp(argname, "cache_delta") == 0) {
			errno = 0;
			temp_val = strtoll(argval, &num_check_ptr, 10);
			if ((errno != 0) || (*num_check_ptr != '\0')) {
				fclose(fptr);
				printf("Number conversion error\n");
				return -1;
			}
			CACHE_DELTA = temp_val;
			continue;
		}
		if (strcasecmp(argname, "max_block_size") == 0) {
			errno = 0;
			temp_val = strtoll(argval, &num_check_ptr, 10);
			if ((errno != 0) || (*num_check_ptr != '\0')) {
				fclose(fptr);
				printf("Number conversion error\n");
				return -1;
			}
			MAX_BLOCK_SIZE = temp_val;
			continue;
		}
	}
	fclose(fptr);

	return 0;
}

/************************************************************************
*
* Function name: validate_system_config
*        Inputs: None
*       Summary: Validate system configuration.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int validate_system_config(void)
{
	FILE *fptr;
	char pathname[400];
	char tempval[10];
	int ret_val;

	printf("%s 1\n%s 2\n%s 3\n%s 4\n%s 5\n", METAPATH, BLOCKPATH,
					SUPERBLOCK, UNCLAIMEDFILE, HCFSSYSTEM);
	printf("%lld %lld %lld %lld\n", CACHE_SOFT_LIMIT, CACHE_HARD_LIMIT,
					CACHE_DELTA, MAX_BLOCK_SIZE);

	sprintf(pathname, "%s/testfile", BLOCKPATH);

	fptr = fopen(pathname, "w");
	fprintf(fptr, "test\n");
	fclose(fptr);

	ret_val = setxattr(pathname, "user.dirty", "T", 1, 0);
	if (ret_val < 0) {
		printf("Needs support for extended attributes, error no: %d\n",
								errno);
		return -1;
	}

	tempval[0] = 0;
	getxattr(pathname, "user.dirty", (void *) tempval, 1);
	printf("test value is: %s, %d\n", tempval, strncmp(tempval, "T", 1));
	unlink(pathname);

	/* TODO: Complete system config validation */

	return 0;
}

/************************************************************************
*
* Function name: check_file_size
*        Inputs: const char *path
*       Summary: Check the file length of "path.
*  Return value: File length in bytes if successful. Otherwise returns -1.
*
*************************************************************************/
off_t check_file_size(const char *path)
{
	struct stat block_stat;

	if (stat(path, &block_stat) == 0)
		return block_stat.st_size;
	else
		return -1;
}

/************************************************************************
*
* Function name: change_system_meta
*        Inputs: long long system_size_delta, long long cache_size_delta,
*                long long cache_blocks_delta
*       Summary: Update system meta (total volume size, cache size, num
*                of cache entries) and sync to disk.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int change_system_meta(long long system_size_delta,
	long long cache_size_delta, long long cache_blocks_delta)
{
	sem_wait(&(hcfs_system->access_sem));
	hcfs_system->systemdata.system_size += system_size_delta;
	if (hcfs_system->systemdata.system_size < 0)
		hcfs_system->systemdata.system_size = 0;
	hcfs_system->systemdata.cache_size += cache_size_delta;
	if (hcfs_system->systemdata.cache_size < 0)
		hcfs_system->systemdata.cache_size = 0;
	hcfs_system->systemdata.cache_blocks += cache_blocks_delta;
	if (hcfs_system->systemdata.cache_blocks < 0)
		hcfs_system->systemdata.cache_blocks = 0;
	sync_hcfs_system_data(FALSE);
	sem_post(&(hcfs_system->access_sem));

	return 0;
}

