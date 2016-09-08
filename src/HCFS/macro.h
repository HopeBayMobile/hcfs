/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: macro.h
* Abstract: The c header file for defining macros used in HCFS.
*
* Revision History
* 2015/5/29 Jiahong created this file.
*
**************************************************************************/

#ifndef SRC_HCFS_MACRO_H_
#define SRC_HCFS_MACRO_H_

#include "logger.h"
#include "utils.h"
#include "params.h"

#if defined(__GNUC__)
#define _PACKED __attribute__((packed))
#define _UNUSED __attribute__((unused))
#else
#define _PACKED
#define _UNUSED
#endif

#define S_ISFILE(mode) (S_ISREG(mode) || S_ISFIFO(mode) || S_ISSOCK(mode))

#define UNUSED(x) ((void)x)

#define FSEEK(A, B, C)\
	do {\
		errcode = 0;\
		ret = fseek(A, B, C);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__, \
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	} while (0)

#define FSYNC(A)\
	do {\
		errcode = 0;\
		ret = fsync(A);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__, \
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	} while (0)

#define FTRUNCATE(A, B)\
	do {\
		errcode = 0;\
		ret = ftruncate(A, B);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__, \
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	} while (0)

#define SETXATTR(A, B, C, D, E)\
	do {\
		errcode = 0;\
		ret = setxattr(A, B, C, D, E);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__, \
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	} while (0)

#define FSETXATTR(A, B, C, D, E)\
	do {\
		errcode = 0;\
		ret = fsetxattr(A, B, C, D, E);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__, \
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	} while (0)

#define LSEEK(A, B, C)\
	do {\
		errcode = 0;\
		ret_pos = lseek(A, B, C);\
		if (ret_pos == (off_t) -1) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__, \
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	} while (0)

#define FTELL(A)\
	do {\
		errcode = 0;\
		ret_pos = (int64_t) ftell(A);\
		if (ret_pos < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__, \
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	} while (0)

#define UNLINK(A)\
	do {\
		errcode = 0;\
		ret = unlink(A);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__, \
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	} while (0)

#define MKDIR(A, B)\
	do {\
		errcode = 0;\
		ret = mkdir(A, B);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__, \
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	} while (0)

#define MKNOD(A, B, C)\
	do {\
		errcode = 0;\
		ret = mknod(A, B, C);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s, %d\n", __func__,\
				errcode, strerror(errcode), __LINE__);\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	} while (0)

#define FREAD(A, B, C, D)\
	do {\
		errcode = 0;\
		ret_size = fread(A, B, C, D);\
		if ((ret_size < C) && ((errcode = ferror(D)) != 0)) {\
			clearerr(D);\
			write_log(0, "IO error in %s. Code %d\n", __func__,\
			          errcode);\
			errcode = -EIO;\
			goto errcode_handle;\
		} \
	} while (0)

#define FWRITE(A, B, C, D)\
	do {\
		errcode = 0;\
		ret_size = fwrite(A, B, C, D);\
		if (((ssize_t)ret_size < (ssize_t)C) &&\
		    ((errcode = ferror(D)) != 0)) {\
			clearerr(D);\
			write_log(0, "IO error in %s. Code %d\n", __func__,\
			          errcode);\
			errcode = -EIO;\
			goto errcode_handle;\
		} \
	} while (0)

#define PREAD(A, B, C, D)\
	do {\
		errcode = 0;\
		ret_ssize = pread(A, B, C, D);\
		if (ret_ssize < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__, \
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
		if (ret_ssize == 0)\
			memset(B, 0, C);\
	} while (0)

#define PWRITE(A, B, C, D)\
	do {\
		errcode = 0;\
		ret_ssize = pwrite(A, B, C, D);\
		if (ret_ssize < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__, \
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	} while (0)

#define ATOL(A)\
	do {\
		errno = 0;\
		endptr = 0;\
		ret_num = strtol(A, &endptr, 10);\
		errcode = errno;\
		if (errcode != 0) {\
			write_log(0, "Conversion error in %s. Code %d, %s\n", \
				__func__, errcode, strerror(errcode));\
			goto errcode_handle;\
		} \
		if ((endptr != 0) && (*endptr != '\0')) {\
			write_log(0, "Conversion error in %s.\n", \
				__func__);\
			goto errcode_handle;\
		} \
	} while (0)

#define HTTP_PERFORM_RETRY(A)\
	do {\
		num_retries = 0;\
		while (num_retries < MAX_RETRIES) {\
			res = curl_easy_perform(A);\
			if ((res == CURLE_OPERATION_TIMEDOUT) &&\
			    (hcfs_system->backend_is_online == TRUE)) {\
				num_retries++;\
				if (num_retries < MAX_RETRIES)\
					continue;\
			} else {\
				break;\
			} \
		} \
	} while (0)


#define TIMEIT(A)\
	do {\
		gettimeofday(&start, NULL);\
		A;\
		gettimeofday(&stop, NULL);\
		timersub(&stop, &start, &diff);\
		time_spent = diff.tv_sec + (double)diff.tv_usec/1000000;\
	} while (0)

#define FREE(ptr)                                                              \
	do {                                                                   \
		if ((ptr) != NULL) {                                           \
			free(ptr);                                             \
			(ptr) = NULL;                                          \
		}                                                              \
	} while (0)

#endif  /* SRC_HCFS_MACRO_H_ */
