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
* 2015/5/27 Jiahong working on improving error handling
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
	int errcode;

	if (METAPATH == NULL)
		return -1;

	/* Creates meta path if it does not exist */
	if (access(METAPATH, F_OK) == -1) {
		ret_code = mkdir(METAPATH, 0700);
		if (ret_code < 0) {
			errcode = errno;
			printf("Error in creating meta dir. Code %d, %s\n",
					errcode, strerror(errcode));
			return -errcode;
		}
	}

	sub_dir = this_inode % NUMSUBDIR;
	snprintf(tempname, METAPATHLEN, "%s/sub_%d", METAPATH, sub_dir);

	/* Creates meta path for meta subfolder if it does not exist */
	if (access(tempname, F_OK) == -1) {
		ret_code = mkdir(tempname, 0700);
		if (ret_code < 0) {
			errcode = errno;
			printf("Error in creating meta dir. Code %d, %s\n",
					errcode, strerror(errcode));
			return -errcode;
		}
	}

	snprintf(pathname, METAPATHLEN, "%s/sub_%d/meta%ld",
		METAPATH, sub_dir, this_inode);

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
	char tempname[METAPATHLEN];
	int sub_dir;
	int ret_code = 0;
	int errcode;

	if (METAPATH == NULL)
		return -EPERM;

	if (access(METAPATH, F_OK) == -1) {
		ret_code = mkdir(METAPATH, 0700);
		if (ret_code < 0) {
			errcode = errno;
			printf("Error in creating meta dir. Code %d, %s\n",
					errcode, strerror(errcode));
			return -errcode;
		}
	}

	sub_dir = this_inode % NUMSUBDIR;
	snprintf(tempname, METAPATHLEN, "%s/todelete", METAPATH);
	if (access(tempname, F_OK) == -1) {
		ret_code = mkdir(tempname, 0700);
		if (ret_code < 0) {
			errcode = errno;
			printf("Error in creating meta dir. Code %d, %s\n",
					errcode, strerror(errcode));
			return -errcode;
		}
	}
	snprintf(tempname, METAPATHLEN, "%s/todelete/sub_%d",
				METAPATH, sub_dir);
	if (access(tempname, F_OK) == -1) {
		ret_code = mkdir(tempname, 0700);
		if (ret_code < 0) {
			errcode = errno;
			printf("Error in creating meta dir. Code %d, %s\n",
					errcode, strerror(errcode));
			return -errcode;
		}
	}
	snprintf(pathname, METAPATHLEN, "%s/todelete/sub_%d/meta%ld",
			METAPATH, sub_dir, this_inode);
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
	char tempname[BLOCKPATHLEN];
	int sub_dir;
	int ret_code = 0;
	int errcode;

	if (BLOCKPATH == NULL)
		return -EPERM;

	if (access(BLOCKPATH, F_OK) == -1) {
		ret_code = mkdir(BLOCKPATH, 0700);
		if (ret_code < 0) {
			errcode = errno;
			printf("Error in creating cache dir. Code %d, %s\n",
					errcode, strerror(errcode));
			return -errcode;
		}
	}

	sub_dir = (this_inode + block_num) % NUMSUBDIR;
	snprintf(tempname, BLOCKPATHLEN, "%s/sub_%d", BLOCKPATH, sub_dir);
	if (access(tempname, F_OK) == -1) {
		ret_code = mkdir(tempname, 0700);
		if (ret_code < 0) {
			errcode = errno;
			printf("Error in creating cache dir. Code %d, %s\n",
					errcode, strerror(errcode));
			return -errcode;
		}
	}
	snprintf(pathname, BLOCKPATHLEN, "%s/sub_%d/block%ld_%lld",
			BLOCKPATH, sub_dir, this_inode, block_num);

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
/* TODO: remove this function if no further use. */
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
	char argname[200], argval[200], *tokptr1, *toktmp, *strptr;
	long long temp_val;
	int tmp_len;
	int errcode;

	fptr = fopen(config_path, "r");

	if (fptr == NULL) {
		errcode = errno;
		printf("Cannot open config file (%s) for reading\n",
								config_path);
		printf("Code %d, %s\n", errcode, strerror(errcode));
		return -1;
	}

	CURRENT_BACKEND = -1;

	while (!feof(fptr)) {
		ret_ptr = fgets(tempbuf, 180, fptr);
		if (ret_ptr == NULL) {
			if (ferror(fptr) != 0) {
				printf("Error while reading config file. ");
				printf("Aborting.\n");
				fclose(fptr);
				return -1;
			}
			break;
		}

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
			if (METAPATH == NULL) {
				printf("Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			strcpy(METAPATH, argval);
			continue;
		}
		if (strcasecmp(argname, "blockpath") == 0) {
			BLOCKPATH = (char *) malloc(strlen(argval) + 10);
			if (BLOCKPATH == NULL) {
				printf("Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}

			strcpy(BLOCKPATH, argval);
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
		if (strcasecmp(argname, "current_backend") == 0) {
			CURRENT_BACKEND = -1;
			if (strcasecmp(argval, "SWIFT") == 0)
				CURRENT_BACKEND = SWIFT;
			if (strcasecmp(argval, "S3") == 0)
				CURRENT_BACKEND = S3;
			if (CURRENT_BACKEND == -1) {
				fclose(fptr);
				printf("Unsupported backend\n");
				return -1;
			}
			continue;
		}
		if (strcasecmp(argname, "swift_account") == 0) {
			SWIFT_ACCOUNT = (char *) malloc(strlen(argval) + 10);
			if (SWIFT_ACCOUNT == NULL) {
				printf("Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}

			snprintf(SWIFT_ACCOUNT, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "swift_user") == 0) {
			SWIFT_USER = (char *) malloc(strlen(argval) + 10);
			if (SWIFT_USER == NULL) {
				printf("Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(SWIFT_USER, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "swift_pass") == 0) {
			SWIFT_PASS = (char *) malloc(strlen(argval) + 10);
			if (SWIFT_PASS == NULL) {
				printf("Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(SWIFT_PASS, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "swift_url") == 0) {
			SWIFT_URL = (char *) malloc(strlen(argval) + 10);
			if (SWIFT_URL == NULL) {
				printf("Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(SWIFT_URL, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "swift_container") == 0) {
			SWIFT_CONTAINER = (char *) malloc(strlen(argval) + 10);
			if (SWIFT_CONTAINER == NULL) {
				printf("Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(SWIFT_CONTAINER, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "swift_protocol") == 0) {
			if ((strcasecmp(argval, "HTTP") != 0) &&
				(strcasecmp(argval, "HTTPS") != 0)) {
				fclose(fptr);
				printf("Unsupported protocol\n");
				return -1;
			}
			SWIFT_PROTOCOL = (char *) malloc(strlen(argval) + 10);
			if (SWIFT_PROTOCOL == NULL) {
				printf("Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(SWIFT_PROTOCOL, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "s3_access") == 0) {
			S3_ACCESS = (char *) malloc(strlen(argval) + 10);
			if (S3_ACCESS == NULL) {
				printf("Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(S3_ACCESS, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "s3_secret") == 0) {
			S3_SECRET = (char *) malloc(strlen(argval) + 10);
			if (S3_SECRET == NULL) {
				printf("Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(S3_SECRET, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "s3_url") == 0) {
			S3_URL = (char *) malloc(strlen(argval) + 10);
			if (S3_URL == NULL) {
				printf("Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(S3_URL, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "s3_bucket") == 0) {
			S3_BUCKET = (char *) malloc(strlen(argval) + 10);
			if (S3_BUCKET == NULL) {
				printf("Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(S3_BUCKET, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "s3_protocol") == 0) {
			if ((strcasecmp(argval, "HTTP") != 0) &&
				(strcasecmp(argval, "HTTPS") != 0)) {
				fclose(fptr);
				printf("Unsupported protocol\n");
				return -1;
			}
			S3_PROTOCOL = (char *) malloc(strlen(argval) + 10);
			if (S3_PROTOCOL == NULL) {
				printf("Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(S3_PROTOCOL, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
	}

	if (((S3_URL != NULL) && (S3_PROTOCOL != NULL))
				&& (S3_BUCKET != NULL)) {
		tmp_len = strlen(S3_URL) + strlen(S3_PROTOCOL)
					+ strlen(S3_BUCKET) + 20;
		S3_BUCKET_URL = (char *) malloc(tmp_len);
		if (S3_BUCKET_URL == NULL) {
			printf("Out of memory when reading config\n");
			fclose(fptr);
			return -1;
		}
		snprintf(S3_BUCKET_URL, tmp_len, "%s://%s.%s",
				S3_PROTOCOL, S3_BUCKET, S3_URL);
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
	int errcode;

	/* Validating system path settings */

	if (CURRENT_BACKEND < 0) {
		printf("Backend selection does not exist\n");
		return -1;
	}

	if (access(METAPATH, F_OK) != 0) {
		printf("Meta path does not exist. Aborting\n");
		return -1;
	}

	if (access(BLOCKPATH, F_OK) != 0) {
		printf("Block cache path does not exist. Aborting\n");
		return -1;
	}

	sprintf(pathname, "%s/testfile", BLOCKPATH);

	fptr = fopen(pathname, "w");
	if (fptr == NULL) {
		errcode = errno;
		printf("Error when testing cache dir writing. Code %d, %s\n",
				errcode, strerror(errcode));
		return -1;
	}
	fprintf(fptr, "test\n");
	fclose(fptr);

	ret_val = setxattr(pathname, "user.dirty", "T", 1, 0);
	if (ret_val < 0) {
		errcode = errno;
		printf("Needs support for extended attributes, error no: %d\n",
								errcode);
		return -1;
	}

	tempval[0] = 0;
	ret_val = getxattr(pathname, "user.dirty", (void *) tempval, 1);
	if (ret_val < 0) {
		errcode = errno;
		printf("Needs support for extended attributes, error no: %d\n",
								errcode);
		return -1;
	}
	printf("test value is: %s, %d\n", tempval, strncmp(tempval, "T", 1));
	unlink(pathname);

	SUPERBLOCK = (char *) malloc(strlen(METAPATH) + 20);
	if (SUPERBLOCK == NULL) {
		printf("Out of memory\n");
		return -1;
	}
	snprintf(SUPERBLOCK, strlen(METAPATH) + 20, "%s/superblock",
			METAPATH);

	UNCLAIMEDFILE = (char *) malloc(strlen(METAPATH) + 20);
	if (UNCLAIMEDFILE == NULL) {
		printf("Out of memory\n");
		return -1;
	}
	snprintf(UNCLAIMEDFILE, strlen(METAPATH) + 20, "%s/unclaimedlist",
			METAPATH);

	HCFSSYSTEM = (char *) malloc(strlen(METAPATH) + 20);
	if (HCFSSYSTEM == NULL) {
		printf("Out of memory\n");
		return -1;
	}
	snprintf(HCFSSYSTEM, strlen(METAPATH) + 20, "%s/hcfssystemfile",
			METAPATH);

	/* Validating cache and block settings */
	/* TODO: If system already created, need to check if block size
		is changed, or just use existing block size. */
	/* TODO: For cache size, perhaps need to check against space
		already used on the target disk (or adjust dynamically).*/

	if (MAX_BLOCK_SIZE <= 0) {
		printf("Block size cannot be zero or less\n");
		return -1;
	}
	if (CACHE_DELTA < MAX_BLOCK_SIZE) {
		printf("cache_delta must be at least max_block_size\n");
		return -1;
	}
	if (CACHE_SOFT_LIMIT < MAX_BLOCK_SIZE) {
		printf("cache_soft_limit must be at least max_block_size\n");
		return -1;
	}
	if (CACHE_HARD_LIMIT < CACHE_SOFT_LIMIT) {
		printf("cache_hard_limit must be at least cache_soft_limit\n");
		return -1;
	}

	/* Validate that the information for the assigned backend
		is complete. */
	/* TODO: Maybe move format checking of backend settings here, and
		also connection testing. */

	if (CURRENT_BACKEND == SWIFT) {
		if (SWIFT_ACCOUNT == NULL) {
			printf("Swift account missing from configuration\n");
			return -1;
		}
		if (SWIFT_USER == NULL) {
			printf("Swift user missing from configuration\n");
			return -1;
		}
		if (SWIFT_PASS == NULL) {
			printf("Swift password missing from configuration\n");
			return -1;
		}
		if (SWIFT_URL == NULL) {
			printf("Swift URL missing from configuration\n");
			return -1;
		}
		if (SWIFT_CONTAINER == NULL) {
			printf("Swift container missing from configuration\n");
			return -1;
		}
		if (SWIFT_PROTOCOL == NULL) {
			printf("Swift protocol missing from configuration\n");
			return -1;
		}
	}

	if (CURRENT_BACKEND == S3) {
		if (S3_ACCESS == NULL) {
			printf("S3 access key missing from configuration\n");
			return -1;
		}
		if (S3_SECRET == NULL) {
			printf("S3 secret key missing from configuration\n");
			return -1;
		}
		if (S3_URL == NULL) {
			printf("S3 URL missing from configuration\n");
			return -1;
		}
		if (S3_BUCKET == NULL) {
			printf("S3 bucket missing from configuration\n");
			return -1;
		}
		if (S3_PROTOCOL == NULL) {
			printf("S3 protocol missing from configuration\n");
			return -1;
		}
	}

	printf("%s 1\n%s 2\n%s 3\n%s 4\n%s 5\n", METAPATH, BLOCKPATH,
					SUPERBLOCK, UNCLAIMEDFILE, HCFSSYSTEM);
	printf("%lld %lld %lld %lld\n", CACHE_SOFT_LIMIT, CACHE_HARD_LIMIT,
					CACHE_DELTA, MAX_BLOCK_SIZE);

	return 0;
}

/************************************************************************
*
* Function name: check_file_size
*        Inputs: const char *path
*       Summary: Check the file length of "path.
*  Return value: File length in bytes if successful. Otherwise returns
*                negation of error code.
*
*************************************************************************/
off_t check_file_size(const char *path)
{
	struct stat block_stat;
	int errcode;

	errcode = stat(path, &block_stat);
	if (errcode == 0)
		return block_stat.st_size;
	errcode = errno;
	printf("Error when checking file size. Code %d, %s\n",
			errcode, strerror(errcode));
	return -errcode;
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

