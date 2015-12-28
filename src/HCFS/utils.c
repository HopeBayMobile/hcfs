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

#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#ifndef _ANDROID_ENV_
#include <attr/xattr.h>
#endif
#include <inttypes.h>

#include "global.h"
#include "fuseop.h"
#include "params.h"
#include "hfuse_system.h"
#include "macro.h"
#include "logger.h"
#include "hcfs_tocloud.h"
#include "hcfs_clouddelete.h"
#include "hcfs_cacheops.h"
#include "monitor.h"

SYSTEM_CONF_STRUCT *system_config = NULL;

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
	int errcode, ret;

	if (METAPATH == NULL)
		return -1;

	/* Creates meta path if it does not exist */
	if (access(METAPATH, F_OK) == -1)
		MKDIR(METAPATH, 0700);

	sub_dir = this_inode % NUMSUBDIR;
	snprintf(tempname, METAPATHLEN, "%s/sub_%d", METAPATH, sub_dir);

	/* Creates meta path for meta subfolder if it does not exist */
	if (access(tempname, F_OK) == -1)
		MKDIR(tempname, 0700);

	snprintf(pathname, METAPATHLEN, "%s/sub_%d/meta%" PRIu64 "",
		METAPATH, sub_dir, (uint64_t)this_inode);

	return 0;
errcode_handle:
	return errcode;
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
	int errcode, ret;

	if (METAPATH == NULL)
		return -EPERM;

	if (access(METAPATH, F_OK) == -1)
		MKDIR(METAPATH, 0700);

	sub_dir = this_inode % NUMSUBDIR;
	snprintf(tempname, METAPATHLEN, "%s/todelete", METAPATH);
	if (access(tempname, F_OK) == -1)
		MKDIR(tempname, 0700);

	snprintf(tempname, METAPATHLEN, "%s/todelete/sub_%d",
				METAPATH, sub_dir);
	if (access(tempname, F_OK) == -1)
		MKDIR(tempname, 0700);

	snprintf(pathname, METAPATHLEN, "%s/todelete/sub_%d/meta%" PRIu64 "",
			METAPATH, sub_dir, (uint64_t)this_inode);
	return 0;
errcode_handle:
	return errcode;
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
	int errcode, ret;

	if (BLOCKPATH == NULL)
		return -EPERM;

	if (access(BLOCKPATH, F_OK) == -1)
		MKDIR(BLOCKPATH, 0700);

	sub_dir = (this_inode + block_num) % NUMSUBDIR;
	snprintf(tempname, BLOCKPATHLEN, "%s/sub_%d", BLOCKPATH, sub_dir);
	if (access(tempname, F_OK) == -1)
		MKDIR(tempname, 0700);

	snprintf(pathname, BLOCKPATHLEN, "%s/sub_%d/block%" PRIu64 "_%lld",
			BLOCKPATH, sub_dir, (uint64_t)this_inode, block_num);

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: fetch_ddt_path
*        Inputs: char *pathname
*        Summary: Use the last character of the hash as the key of ddt
*                 metafile. Copy the path to ddt_btree_meta to "pathname".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int fetch_ddt_path(char *pathname, unsigned char last_char)
{
	char tempname[METAPATHLEN];
	int errcode, ret;

	if (METAPATH == NULL)
		return -1;

	/* Creates meta path if it does not exist */
	if (access(METAPATH, F_OK) == -1)
		MKDIR(METAPATH, 0700);

	snprintf(tempname, METAPATHLEN, "%s/ddt", METAPATH);

	/* Creates meta path for meta subfolder if it does not exist */
	if (access(tempname, F_OK) == -1)
		MKDIR(tempname, 0700);

	snprintf(pathname, METAPATHLEN, "%s/ddt/ddt_meta_%02x",
		METAPATH, last_char);

	return 0;
errcode_handle:
	return errcode;
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
int read_system_config(char *config_path, SYSTEM_CONF_STRUCT *config)
{
	FILE *fptr;
	char tempbuf[200], *ret_ptr, *num_check_ptr;
	char argname[200], argval[200], *tokptr1, *toktmp, *strptr;
	long long temp_val;
	int tmp_len;
	int errcode;

	memset(config, 0, sizeof(SYSTEM_CONF_STRUCT));

	fptr = fopen(config_path, "r");

	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "Cannot open config file (%s) for reading\n",
								config_path);
		write_log(0, "Code %d, %s\n", errcode, strerror(errcode));
		return -1;
	}

	config->current_backend = -1;
	config->log_level = 0;
	config->log_path = NULL;

	while (!feof(fptr)) {
		ret_ptr = fgets(tempbuf, 180, fptr);
		if (ret_ptr == NULL) {
			if (ferror(fptr) != 0) {
				write_log(0,
					"Error while reading config file.");
				write_log(0, "Aborting.\n");
				fclose(fptr);
				return -1;
			}
			break;
		}

		if (strlen(tempbuf) > 170) {
			write_log(0,
				"Length of option value exceeds limit.");
			write_log(0, "(Limit: 170 chars). Exiting.\n");
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

		if (strcasecmp(argname, "log_level") == 0) {
			errno = 0;
			temp_val = strtoll(argval, &num_check_ptr, 10);
			if ((errno != 0) || (*num_check_ptr != '\0')) {
				fclose(fptr);
				write_log(0, "Number conversion error\n");
				return -1;
			}
			if (temp_val < 0) {
				fclose(fptr);
				write_log(0,
					"Log level cannot be less than zero.");
				return -1;
			}
			config->log_level = temp_val;
			continue;
		}

		if (strcasecmp(argname, "log_path") == 0) {
			config->log_path = (char *) malloc(strlen(argval) + 10);
			if (config->log_path == NULL) {
				write_log(0,
					"Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}

			strcpy(config->log_path, argval);
			continue;
		}

		if (strcasecmp(argname, "metapath") == 0) {
			config->metapath = (char *) malloc(strlen(argval) + 10);
			if (config->metapath == NULL) {
				write_log(0,
					"Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			strcpy(config->metapath, argval);
			continue;
		}
		if (strcasecmp(argname, "blockpath") == 0) {
			config->blockpath = (char *) malloc(strlen(argval) + 10);
			if (config->blockpath == NULL) {
				write_log(0,
					"Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}

			strcpy(config->blockpath, argval);
			continue;
		}
		if (strcasecmp(argname, "cache_soft_limit") == 0) {
			errno = 0;
			temp_val = strtoll(argval, &num_check_ptr, 10);
			if ((errno != 0) || (*num_check_ptr != '\0')) {
				fclose(fptr);
				write_log(0, "Number conversion error\n");
				return -1;
			}
			config->cache_soft_limit = temp_val;
			continue;
		}
		if (strcasecmp(argname, "cache_hard_limit") == 0) {
			errno = 0;
			temp_val = strtoll(argval, &num_check_ptr, 10);
			if ((errno != 0) || (*num_check_ptr != '\0')) {
				fclose(fptr);
				write_log(0, "Number conversion error\n");
				return -1;
			}
			config->cache_hard_limit = temp_val;
			continue;
		}
		if (strcasecmp(argname, "cache_delta") == 0) {
			errno = 0;
			temp_val = strtoll(argval, &num_check_ptr, 10);
			if ((errno != 0) || (*num_check_ptr != '\0')) {
				fclose(fptr);
				write_log(0, "Number conversion error\n");
				return -1;
			}
			config->cache_update_delta = temp_val;
			continue;
		}
		if (strcasecmp(argname, "max_block_size") == 0) {
			errno = 0;
			temp_val = strtoll(argval, &num_check_ptr, 10);
			if ((errno != 0) || (*num_check_ptr != '\0')) {
				fclose(fptr);
				write_log(0, "Number conversion error\n");
				return -1;
			}
			config->max_block_size = temp_val;
			continue;
		}
		if (strcasecmp(argname, "current_backend") == 0) {
			config->current_backend = -1;
			if (strcasecmp(argval, "SWIFT") == 0)
				config->current_backend = SWIFT;
			if (strcasecmp(argval, "S3") == 0)
				config->current_backend = S3;
			if (strcasecmp(argval, "NONE") == 0)
				config->current_backend = NONE;
			if (config->current_backend == -1) {
				fclose(fptr);
				write_log(0, "Unsupported backend\n");
				return -1;
			}
			continue;
		}
		if (strcasecmp(argname, "swift_account") == 0) {
			config->swift_account = (char *) malloc(strlen(argval) + 10);
			if (config->swift_account == NULL) {
				write_log(0,
					"Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}

			snprintf(config->swift_account, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "swift_user") == 0) {
			config->swift_user = (char *) malloc(strlen(argval) + 10);
			if (config->swift_user == NULL) {
				write_log(0,
					"Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(config->swift_user, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "swift_pass") == 0) {
			config->swift_pass = (char *) malloc(strlen(argval) + 10);
			if (config->swift_pass == NULL) {
				write_log(0,
					"Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(config->swift_pass, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "swift_url") == 0) {
			config->swift_url = (char *) malloc(strlen(argval) + 10);
			if (config->swift_url == NULL) {
				write_log(0,
					"Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(config->swift_url, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "swift_container") == 0) {
			config->swift_container = (char *) malloc(strlen(argval) + 10);
			if (config->swift_container == NULL) {
				write_log(0,
					"Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(config->swift_container, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "swift_protocol") == 0) {
			if ((strcasecmp(argval, "HTTP") != 0) &&
				(strcasecmp(argval, "HTTPS") != 0)) {
				fclose(fptr);
				write_log(0, "Unsupported protocol\n");
				return -1;
			}
			config->swift_protocol = (char *) malloc(strlen(argval) + 10);
			if (config->swift_protocol == NULL) {
				write_log(0,
					"Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(config->swift_protocol, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "s3_access") == 0) {
			config->s3_access = (char *) malloc(strlen(argval) + 10);
			if (config->s3_access == NULL) {
				write_log(0,
					"Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(config->s3_access, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "s3_secret") == 0) {
			config->s3_secret = (char *) malloc(strlen(argval) + 10);
			if (config->s3_secret == NULL) {
				write_log(0,
					"Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(config->s3_secret, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "s3_url") == 0) {
			config->s3_url = (char *) malloc(strlen(argval) + 10);
			if (config->s3_url == NULL) {
				write_log(0,
					"Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(config->s3_url, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "s3_bucket") == 0) {
			config->s3_bucket = (char *) malloc(strlen(argval) + 10);
			if (config->s3_bucket == NULL) {
				write_log(0,
					"Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(config->s3_bucket, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
		if (strcasecmp(argname, "s3_protocol") == 0) {
			if ((strcasecmp(argval, "HTTP") != 0) &&
				(strcasecmp(argval, "HTTPS") != 0)) {
				fclose(fptr);
				write_log(0, "Unsupported protocol\n");
				return -1;
			}
			config->s3_protocol = (char *) malloc(strlen(argval) + 10);
			if (config->s3_protocol == NULL) {
				write_log(0,
					"Out of memory when reading config\n");
				fclose(fptr);
				return -1;
			}
			snprintf(config->s3_protocol, strlen(argval) + 10,
				"%s", argval);
			continue;
		}
	}

	if (((config->s3_url != NULL) && (config->s3_protocol != NULL))
				&& (config->s3_bucket != NULL)) {
		tmp_len = strlen(config->s3_url) + strlen(config->s3_protocol)
					+ strlen(config->s3_bucket) + 20;
		config->s3_bucket_url = (char *) malloc(tmp_len);
		if (config->s3_bucket_url == NULL) {
			write_log(0, "Out of memory when reading config\n");
			fclose(fptr);
			return -1;
		}
		snprintf(config->s3_bucket_url, tmp_len, "%s://%s.%s",
				config->s3_protocol, config->s3_bucket, config->s3_url);
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
int validate_system_config(SYSTEM_CONF_STRUCT *config)
{
	FILE *fptr;
	char pathname[400];
#ifndef _ANDROID_ENV_
	char tempval[10];
#endif
	int ret_val;
	int errcode;

	/* Validating system path settings */

	if (config->current_backend < 0) {
		write_log(0, "Backend selection does not exist\n");
		return -1;
	}

	/* Write log to current path if log path is invalid */
	if (config->log_path != NULL) {
		if (access(config->log_path, F_OK) != 0) {
			write_log(0, "Cannot access log path %s. %s", config->log_path,
				"Default write log to current path\n");
			config->log_path = NULL;
		} else {
			sprintf(pathname, "%s/testfile", config->log_path);
			fptr = fopen(pathname, "w");
			if (fptr == NULL) {
				errcode = errno;
				write_log(0, "%s Code %d, %s\n",
					  "Error when testing log dir writing.",
					  errcode, strerror(errcode));
				write_log(0, "Write to current path\n");
				config->log_path = NULL;
			} else {
				fclose(fptr);
				unlink(pathname);
			}
		}
	}

	if (access(config->metapath, F_OK) != 0) {
		write_log(0, "Meta path does not exist. Aborting\n");
		return -1;
	}

	if (access(config->blockpath, F_OK) != 0) {
		write_log(0, "Block cache path does not exist. Aborting\n");
		return -1;
	}

	sprintf(pathname, "%s/testfile", config->blockpath);

	fptr = fopen(pathname, "w");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0,
			"Error when testing cache dir writing. Code %d, %s\n",
				errcode, strerror(errcode));
		return -1;
	}
	fprintf(fptr, "test\n");
	fclose(fptr);

#ifndef _ANDROID_ENV_
	ret_val = setxattr(pathname, "user.dirty", "T", 1, 0);
	if (ret_val < 0) {
		errcode = errno;
		write_log(0,
			"Needs support for extended attributes, error no: %d\n",
								errcode);
		return -1;
	}

	tempval[0] = 0;
	ret_val = getxattr(pathname, "user.dirty", (void *) tempval, 1);
	if (ret_val < 0) {
		errcode = errno;
		write_log(0,
			"Needs support for extended attributes, error no: %d\n",
								errcode);
		return -1;
	}
	write_log(10,
		"test value is: %s, %d\n", tempval, strncmp(tempval, "T", 1));
#endif

	unlink(pathname);

	config->superblock_name = (char *) malloc(strlen(config->metapath) + 20);
	if (config->superblock_name == NULL) {
		write_log(0, "Out of memory\n");
		return -1;
	}
	snprintf(config->superblock_name, strlen(config->metapath) + 20, "%s/superblock",
			config->metapath);

	config->unclaimed_name = (char *) malloc(strlen(config->metapath) + 20);
	if (config->unclaimed_name == NULL) {
		write_log(0, "Out of memory\n");
		return -1;
	}
	snprintf(config->unclaimed_name, strlen(config->metapath) + 20, "%s/unclaimedlist",
			config->metapath);

	config->hcfssystem_name = (char *) malloc(strlen(config->metapath) + 20);
	if (config->hcfssystem_name == NULL) {
		write_log(0, "Out of memory\n");
		return -1;
	}
	snprintf(config->hcfssystem_name, strlen(config->metapath) + 20, "%s/hcfssystemfile",
			config->metapath);

	config->hcfspausesync_name = (char *)malloc(strlen(config->metapath) + 20);
	if (config->hcfspausesync_name == NULL) {
		write_log(0, "Out of memory\n");
		return -1;
	}
	snprintf(config->hcfspausesync_name, strlen(config->metapath) + 20, "%s/hcfspausesync",
		 config->metapath);

	/* Validating cache and block settings */
	/* TODO: If system already created, need to check if block size
		is changed, or just use existing block size. */
	/* TODO: For cache size, perhaps need to check against space
		already used on the target disk (or adjust dynamically).*/

	if (config->max_block_size <= 0) {
		write_log(0, "Block size cannot be zero or less\n");
		return -1;
	}
	if (config->cache_update_delta < config->max_block_size) {
		write_log(0, "cache_delta must be at least max_block_size\n");
		return -1;
	}
	if (config->cache_soft_limit < config->max_block_size) {
		write_log(0,
			"cache_soft_limit must be at least max_block_size\n");
		return -1;
	}
	if (config->cache_hard_limit < (config->cache_soft_limit + config->cache_update_delta)) {
		write_log(0, "%s%s", "cache_hard_limit >= ",
			  "cache_soft_limit + cache_delta\n");
		return -1;
	}

	/* Validate that the information for the assigned backend
		is complete. */
	/* TODO: Maybe move format checking of backend settings here, and
		also connection testing. */

	if (config->current_backend == SWIFT) {
		if (config->swift_account == NULL) {
			write_log(0,
				"Swift account missing from configuration\n");
			return -1;
		}
		if (config->swift_user == NULL) {
			write_log(0,
				"Swift user missing from configuration\n");
			return -1;
		}
		if (config->swift_pass == NULL) {
			write_log(0,
				"Swift password missing from configuration\n");
			return -1;
		}
		if (config->swift_url == NULL) {
			write_log(0,
				"Swift URL missing from configuration\n");
			return -1;
		}
		if (config->swift_container == NULL) {
			write_log(0,
				"Swift container missing from configuration\n");
			return -1;
		}
		if (config->swift_protocol == NULL) {
			write_log(0,
				"Swift protocol missing from configuration\n");
			return -1;
		}
	}

	if (config->current_backend == S3) {
		if (config->s3_access == NULL) {
			write_log(0,
				"S3 access key missing from configuration\n");
			return -1;
		}
		if (config->s3_secret == NULL) {
			write_log(0,
				"S3 secret key missing from configuration\n");
			return -1;
		}
		if (config->s3_url == NULL) {
			write_log(0,
				"S3 URL missing from configuration\n");
			return -1;
		}
		if (config->s3_bucket == NULL) {
			write_log(0,
				"S3 bucket missing from configuration\n");
			return -1;
		}
		if (config->s3_protocol == NULL) {
			write_log(0,
				"S3 protocol missing from configuration\n");
			return -1;
		}
	}

	write_log(10, "%s %s\n%s %s\n%s %s\n%s %s\n%s %s\n", "METAPATH",
		  config->metapath, "BLOCKPATH", config->blockpath,
		  "SUPERBLOCK", config->superblock_name,
		  "UNCLAIMEDFILE", config->unclaimed_name,
		  "HCFSSYSTEM", config->hcfssystem_name);
	write_log(10,
		"%lld %lld %lld %lld\n", config->cache_soft_limit,
		config->cache_hard_limit, config->cache_update_delta,
		config->max_block_size);

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
	write_log(0, "Error when checking file size. Code %d, %s\n",
			errcode, strerror(errcode));
	return -errcode;
}

/************************************************************************
*
* Function name: change_system_meta
*        Inputs: long long system_size_delta, long long cache_size_delta,
*                long long cache_blocks_delta, long long dirty_cache_delta
*       Summary: Update system meta (total volume size, cache size, num
*                of cache entries, dirty cache size) and sync to disk.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int change_system_meta(long long system_size_delta,
	long long cache_size_delta, long long cache_blocks_delta,
	long long dirty_cache_delta)
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
	hcfs_system->systemdata.dirty_cache_size += dirty_cache_delta;
	if (hcfs_system->systemdata.dirty_cache_size < 0)
		hcfs_system->systemdata.dirty_cache_size = 0;
	sync_hcfs_system_data(FALSE);
	sem_post(&(hcfs_system->access_sem));

	return 0;
}

int set_block_dirty_status(char *path, FILE *fptr, char status)
{
	int ret, errcode;

#ifdef _ANDROID_ENV_

	struct stat tmpstat;
	if (path != NULL) {
		ret = stat(path, &tmpstat);
		if (ret < 0) {
			errcode = errno;
			write_log(0, "Unexpected IO error\n");
			write_log(10, "In %s. code %d, %s\n", __func__,
				errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		/* Use sticky bit to store dirty status */
		if ((status == TRUE) && ((tmpstat.st_mode & S_ISVTX) == 0)) {
			ret = chmod(path, tmpstat.st_mode | S_ISVTX);
			if (ret < 0) {
				errcode = errno;
				write_log(0, "Unexpected IO error\n");
				write_log(10, "In %s. code %d, %s\n", __func__,
					errcode, strerror(errcode));
				errcode = -errcode;
				goto errcode_handle;
			}
		}

		if ((status == FALSE) && ((tmpstat.st_mode & S_ISVTX) != 0)) {
			ret = chmod(path, tmpstat.st_mode & ~S_ISVTX);
			if (ret < 0) {
				errcode = errno;
				write_log(0, "Unexpected IO error\n");
				write_log(10, "In %s. code %d, %s\n", __func__,
					errcode, strerror(errcode));
				errcode = -errcode;
				goto errcode_handle;
			}
		}
	} else if (fptr != NULL) {
		ret = fstat(fileno(fptr), &tmpstat);
		if (ret < 0) {
			errcode = errno;
			write_log(0, "Unexpected IO error\n");
			write_log(10, "In %s. code %d, %s\n", __func__,
				errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		/* Use sticky bit to store dirty status */
		if ((status == TRUE) && ((tmpstat.st_mode & S_ISVTX) == 0)) {
			ret = fchmod(fileno(fptr), tmpstat.st_mode | S_ISVTX);
			if (ret < 0) {
				errcode = errno;
				write_log(0, "Unexpected IO error\n");
				write_log(10, "In %s. code %d, %s\n", __func__,
					errcode, strerror(errcode));
				errcode = -errcode;
				goto errcode_handle;
			}
		}

		if ((status == FALSE) && ((tmpstat.st_mode & S_ISVTX) != 0)) {
			ret = fchmod(fileno(fptr), tmpstat.st_mode & ~S_ISVTX);
			if (ret < 0) {
				errcode = errno;
				write_log(0, "Unexpected IO error\n");
				write_log(10, "In %s. code %d, %s\n", __func__,
					errcode, strerror(errcode));
				errcode = -errcode;
				goto errcode_handle;
			}
		}

	} else {
		/* Cannot set block dirty status */
		write_log(0, "Unexpected error\n");
		write_log(10, "Unable to set block dirty status\n");
		errcode = -ENOTSUP;
		goto errcode_handle;
	}

#else
	if (path != NULL) {
		if (status == TRUE)
			SETXATTR(path, "user.dirty", "T", 1, 0);
		else
			SETXATTR(path, "user.dirty", "F", 1, 0);
	} else if (fptr != NULL) {
		if (status == TRUE)
			FSETXATTR(fileno(fptr), "user.dirty", "T", 1, 0);
		else
			FSETXATTR(fileno(fptr), "user.dirty", "F", 1, 0);
	} else {
		/* Cannot set block dirty status */
		write_log(0, "Unexpected error\n");
		write_log(10, "Unable to set block dirty status\n");
		errcode = -ENOTSUP;
		goto errcode_handle;
	}
#endif

	return 0;
errcode_handle:
	return errcode;
}

int get_block_dirty_status(char *path, FILE *fptr, char *status)
{
	int ret, errcode;
	char tmpstr[5];

#ifdef _ANDROID_ENV_

	struct stat tmpstat;
	if (path != NULL) {
		ret = stat(path, &tmpstat);
		if (ret < 0) {
			errcode = errno;
			write_log(0, "Unexpected IO error\n");
			write_log(10, "In %s. code %d, %s\n", __func__,
				errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		/* Use sticky bit to store dirty status */

		if ((tmpstat.st_mode & S_ISVTX) == 0)
			*status = FALSE;
		else
			*status = TRUE;
	} else if (fptr != NULL) {
		ret = fstat(fileno(fptr), &tmpstat);
		if (ret < 0) {
			errcode = errno;
			write_log(0, "Unexpected IO error\n");
			write_log(10, "In %s. code %d, %s\n", __func__,
				errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		/* Use sticky bit to store dirty status */

		if ((tmpstat.st_mode & S_ISVTX) == 0)
			*status = FALSE;
		else
			*status = TRUE;
	} else {
		/* Cannot get block dirty status */
		write_log(0, "Unexpected error\n");
		write_log(10, "Unable to get block dirty status\n");
		errcode = -ENOTSUP;
		goto errcode_handle;
	}

#else
	if (path != NULL) {
		ret = getxattr(path, "user.dirty", (void *) tmpstr, 1);
		if (ret < 0) {
			errcode = errno;
			write_log(0, "Unexpected IO error\n");
			write_log(10, "In %s. code %d, %s\n", __func__,
				errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		if (strncmp(tmpstr, "T", 1) == 0)
			*status = TRUE;
		else
			*status = FALSE;
	} else if (fptr != NULL) {
		ret = fgetxattr(fileno(fptr), "user.dirty",
				(void *) tmpstr, 1);
		if (ret < 0) {
			errcode = errno;
			write_log(0, "Unexpected IO error\n");
			write_log(10, "In %s. code %d, %s\n", __func__,
				errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		if (strncmp(tmpstr, "T", 1) == 0)
			*status = TRUE;
		else
			*status = FALSE;
	} else {
		/* Cannot get block dirty status */
		write_log(0, "Unexpected error\n");
		write_log(10, "Unable to get block dirty status\n");
		errcode = -ENOTSUP;
		goto errcode_handle;
	}
#endif

	return 0;
errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: fetch_stat_path
*        Inputs: char *pathname, ino_t this_inode
*        Output: Integer
*       Summary: Given the inode number this_inode,
*                copy the filename to the stat file
*                to the space pointed by pathname.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int fetch_stat_path(char *pathname, ino_t this_inode)
{
	char tempname[METAPATHLEN];
	int sub_dir;
	int errcode, ret;

	if (METAPATH == NULL)
		return -EPERM;

	if (access(METAPATH, F_OK) == -1)
		MKDIR(METAPATH, 0700);

	sub_dir = this_inode % NUMSUBDIR;

	snprintf(tempname, METAPATHLEN, "%s/sub_%d", METAPATH, sub_dir);
	if (access(tempname, F_OK) == -1)
		MKDIR(tempname, 0700);

	snprintf(tempname, METAPATHLEN, "%s/sub_%d/stat", METAPATH, sub_dir);
	if (access(tempname, F_OK) == -1)
		MKDIR(tempname, 0700);

	snprintf(pathname, METAPATHLEN, "%s/sub_%d/stat/stat%" PRIu64 "",
			METAPATH, sub_dir, (uint64_t)this_inode);
	return 0;
errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: fetch_trunc_path
*        Inputs: char *pathname, ino_t this_inode
*        Output: Integer
*       Summary: Given the inode number this_inode,
*                copy the filename to the trunc tag file
*                to the space pointed by pathname.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int fetch_trunc_path(char *pathname, ino_t this_inode)
{
	char tempname[METAPATHLEN];
	int sub_dir;
	int errcode, ret;

	if (METAPATH == NULL)
		return -EPERM;

	if (access(METAPATH, F_OK) == -1)
		MKDIR(METAPATH, 0700);

	sub_dir = this_inode % NUMSUBDIR;

	snprintf(tempname, METAPATHLEN, "%s/sub_%d", METAPATH, sub_dir);
	if (access(tempname, F_OK) == -1)
		MKDIR(tempname, 0700);

	snprintf(tempname, METAPATHLEN, "%s/sub_%d/trunc", METAPATH, sub_dir);
	if (access(tempname, F_OK) == -1)
		MKDIR(tempname, 0700);

	snprintf(pathname, METAPATHLEN, "%s/sub_%d/trunc/trunc%" PRIu64 "",
			METAPATH, sub_dir, (uint64_t)this_inode);
	return 0;
errcode_handle:
	return errcode;
}

/**
 * fetch_error_download_path
 *
 * Fetch an error path employed to record error when failing to download block.
 *
 * @param path  A char type pointer used to get this path.
 * @param inode  Inode number of this block failing to download.
 *
 * @return 0
 */
int fetch_error_download_path(char *path, ino_t inode)
{

	snprintf(path, 200, "/dev/shm/download_error_inode_%"PRIu64"",
			(uint64_t)inode);

	return 0;
}

void get_system_size(long long *cache_size, long long *pinned_size)
{
	sem_wait(&(hcfs_system->access_sem));
	if (cache_size)
		*cache_size = hcfs_system->systemdata.cache_size;
	if (pinned_size)
		*pinned_size = hcfs_system->systemdata.pinned_size;
	sem_post(&(hcfs_system->access_sem));
}

static inline void _translate_storage_location(FILE_STATS_TYPE *in,
					DIR_STATS_TYPE *out)
{
	/* Find out what's the original storage location */
	memset(out, 0, sizeof(DIR_STATS_TYPE));
	if ((in->num_blocks == 0) ||
	    (in->num_blocks == in->num_cached_blocks)) {
		/* If local */
		out->num_local = 1;
	} else if (in->num_cached_blocks == 0) {
		/* If cloud */
		out->num_cloud = 1;
	} else {
		out->num_hybrid = 1;
	}
}

/* Helper for updating per-file statistics in fuseop.c */
int update_file_stats(FILE *metafptr, long long num_blocks_delta,
			long long num_cached_blocks_delta,
			long long cached_size_delta, ino_t thisinode)
{
	int ret, errcode;
	size_t ret_size;
	FILE_STATS_TYPE meta_stats;
	DIR_STATS_TYPE olddirstats, newdirstats, diffstats;

	FSEEK(metafptr, sizeof(struct stat) + sizeof(FILE_META_TYPE),
		SEEK_SET);
	FREAD(&meta_stats, sizeof(FILE_STATS_TYPE), 1, metafptr);

	_translate_storage_location(&meta_stats, &olddirstats);
	meta_stats.num_blocks += num_blocks_delta;
	meta_stats.num_cached_blocks += num_cached_blocks_delta;
	meta_stats.cached_size += cached_size_delta;
	_translate_storage_location(&meta_stats, &newdirstats);

	FSEEK(metafptr, sizeof(struct stat) + sizeof(FILE_META_TYPE),
		SEEK_SET);
	FWRITE(&meta_stats, sizeof(FILE_STATS_TYPE), 1, metafptr);

	diffstats.num_local = newdirstats.num_local - olddirstats.num_local;
	diffstats.num_cloud = newdirstats.num_cloud - olddirstats.num_cloud;
	diffstats.num_hybrid = newdirstats.num_hybrid - olddirstats.num_hybrid;

	/* If the storage location of the file changes, need to update
	all parents to root */
	if ((diffstats.num_local != 0) ||
	    ((diffstats.num_cloud != 0) ||
	     (diffstats.num_hybrid != 0))) {
		ret = update_dirstat_file(thisinode, &diffstats);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}
	}

	return 0;
errcode_handle:
	return errcode;
}

int check_file_storage_location(FILE *fptr,  DIR_STATS_TYPE *newstat)
{
	int ret, errcode;
	size_t ret_size;
	FILE_STATS_TYPE meta_stats;

	FSEEK(fptr, sizeof(struct stat) + sizeof(FILE_META_TYPE),
		SEEK_SET);
	FREAD(&meta_stats, sizeof(FILE_STATS_TYPE), 1, fptr);

	_translate_storage_location(&meta_stats, newstat);

	return 0;
errcode_handle:
	return errcode;
}

void fetch_backend_block_objname(ino_t this_inode, long long block_no,
	char *block_name)
{

#ifdef ARM_32bit_
	sprintf(block_name, "data_%lld_%lld_0", this_inode, block_no);
#else
	sprintf(block_name, "data_%ld_%lld_0", this_inode, block_no);
#endif
	return;
}

void fetch_backend_meta_objname(ino_t this_inode, char *meta_name)
{

#ifdef ARM_32bit_
	sprintf(meta_name, "meta_%lld", this_inode);
#else
	sprintf(meta_name, "meta_%ld", this_inode);
#endif
	return;
}

/**
 * Helper function subjecting to check whether new system config is invalid.
 */
int _check_config(const SYSTEM_CONF_STRUCT *new_config)
{
	if (CURRENT_BACKEND == NONE) /* Always ok when backend is now none */
		return 0;
	else /* When current backend is s3/swift... */
		if (CURRENT_BACKEND != new_config->current_backend)
			return -EINVAL;

	switch (new_config->current_backend) {
	case SWIFT:
		if (strcmp(SWIFT_ACCOUNT, new_config->swift_account))
			return -EINVAL;
		if (strcmp(SWIFT_USER, new_config->swift_user))
			return -EINVAL;
		if (strcmp(SWIFT_PASS, new_config->swift_pass))
			return -EINVAL;
		if (strcmp(SWIFT_URL, new_config->swift_url))
			return -EINVAL;
		if (strcmp(SWIFT_CONTAINER, new_config->swift_container))
			return -EINVAL;
		if (strcmp(SWIFT_PROTOCOL, new_config->swift_protocol))
			return -EINVAL;
		break;
	case S3:
		if (strcmp(S3_ACCESS, new_config->s3_access))
			return -EINVAL;
		if (strcmp(S3_SECRET, new_config->s3_secret))
			return -EINVAL;
		if (strcmp(S3_URL, new_config->s3_url))
			return -EINVAL;
		if (strcmp(S3_BUCKET, new_config->s3_bucket))
			return -EINVAL;
		if (strcmp(S3_PROTOCOL, new_config->s3_protocol))
			return -EINVAL;
		if (strcmp(S3_BUCKET_URL, new_config->s3_bucket_url))
			return -EINVAL;
		break;
	case NONE:
		return -EINVAL;
		break;
	}

	/* Check block size */
	if (MAX_BLOCK_SIZE != new_config->max_block_size)
		return -EINVAL;

	return 0;
}

/**
 * reload_system_config
 *
 * Reload hcfs configuration file. The main purpose of this function is to
 * setup backend information from NONE to swift/s3.
 *
 * @param config_path Path of config file
 *
 * @return 0 on success, otherwise negative error code.
 */
int reload_system_config(const char *config_path)
{
	int ret, count;
	char enable_related_module;
	SYSTEM_CONF_STRUCT *temp_config, *new_config;

	write_log(10, "config path: %s\n", config_path);
	new_config = malloc(sizeof(SYSTEM_CONF_STRUCT));
	if (new_config == NULL)
		return -ENOMEM;

	ret = read_system_config(config_path, new_config);
	if (ret < 0) {
		free(new_config);
		return ret;
	}

	ret = validate_system_config(new_config);
	if (ret < 0) {
		free(new_config);
		return ret;
	}

	/* Compare old config and new config*/
	ret = _check_config(new_config);
	if (ret < 0) {
		free(new_config);
		return ret;
	}

	/* Create backend related threads when backend status from
	 * none to s3/swift */
	enable_related_module = FALSE;
	if ((CURRENT_BACKEND == NONE) && (new_config->current_backend != NONE))
		enable_related_module = TRUE;

	temp_config = system_config;
	system_config = new_config;
	free(temp_config);

	/* Init backend related threads */
	if (enable_related_module == TRUE) {
		ret = prepare_FS_database_backup();
		if (ret < 0) {
			write_log(0, "Error: Fail to prepare FS backup."
				" Code %d\n", -ret);
		}
		init_backend_related_module();
	}

	return 0;
}

void nonblock_sleep(unsigned int secs, BOOL (*wakeup_condition)())
{
	unsigned int count;

	for (count = 0; count < secs; count++) {
		if (wakeup_condition() == TRUE)
			break;
		sleep(1);
	}

	return;
}
