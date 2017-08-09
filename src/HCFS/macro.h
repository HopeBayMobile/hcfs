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

#define MMAP_SIZE (4 * 1024 * 1024)

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

#define RMDIR(A)\
	do {\
		errcode = 0;\
		ret = rmdir(A);\
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

/* _MMAP - B is only implemented on O_WRONLY/O_RDWR. */
#define _MMAP(A, B)\
	({\
	void *addr = NULL;\
	do {\
		UNUSED(ret);\
		errcode = 0;\
		ret = 0;\
		errno = 0;\
		if ((A)->fptr == NULL)\
			break;\
		int fd = fileno((A)->fptr);\
		if (fd == -1)\
			break;\
		off_t off = (B);\
		if (off == 0)\
		{\
			off = lseek(fd, 0, SEEK_END);\
			if (off == -1)\
				break;\
			if (off == 0)\
			{\
				(A)->mmap_addr = NULL;\
				(A)->mmap_len = 0;\
				(A)->mmap_file_len = 0;\
				break;\
			}\
		} else {\
			if (fallocate(fd, 0, 0, off) == -1)\
				break;\
		}\
		/* size_t len = (off+PAGE_SIZE-1)&(~(PAGE_SIZE-1)); */\
		size_t len = (off+MMAP_SIZE-1)&(~(MMAP_SIZE-1));\
		if ((A)->mmap_addr)\
		{\
			addr = mremap((A)->mmap_addr, (A)->mmap_len, len, MREMAP_MAYMOVE);\
		} else {\
			int open_flags;\
			int prot = 0;\
			int flags = 0;\
			open_flags = fcntl(fd, F_GETFL, 0);\
			if (open_flags == -1)\
				break;\
			if (open_flags&O_RDWR)\
			{\
				prot = PROT_READ|PROT_WRITE;\
				flags = MAP_SHARED;\
			} else if (open_flags&O_WRONLY) {\
				if (fcntl(fd, F_SETFL, (open_flags&~O_ACCMODE)|O_RDWR) == -1)\
					break;\
				prot = PROT_READ|PROT_WRITE;\
				flags = MAP_SHARED;\
			} else if (open_flags&O_RDONLY) {\
				prot = PROT_READ;\
				flags = MAP_PRIVATE;\
			}\
			addr = mmap(NULL, len, prot, flags, fd, 0);\
		}\
		if (addr == MAP_FAILED)\
		{\
			addr = NULL;\
			break;\
		}\
		(A)->mmap_addr = addr;\
		(A)->mmap_len = len;\
		(A)->mmap_file_len = off;\
	} while (0);\
	if (errno)\
	{\
		ret = -1;\
		errcode = errno;\
		write_log(0, "IO error in %s. Code %d, %s, %d\n", __func__,\
			errcode, strerror(errcode), __LINE__);\
		errcode = -errcode;\
		goto errcode_handle;\
	}\
	addr;\
	})

#define MMAP(A)\
	do {\
		_MMAP(A, 0);\
	} while (0)

#define MUNMAP(A)\
	do {\
		if ((A)->mmap_addr == NULL)\
			break;\
		munmap((A)->mmap_addr, (A)->mmap_len);\
		(A)->mmap_addr = NULL;\
		(A)->mmap_len = 0;\
		(A)->mmap_file_len = 0;\
	} while (0)

#define MREAD(A, B, C, D)\
	({\
	void *read_ptr = NULL;\
	do {\
		UNUSED(ret_size);\
		errcode = 0;\
		ret_size = 0;\
		void *b = (B);\
		if ((D)+(C) > (A)->mmap_len)\
			MMAP((A));\
		void *p = (A)->mmap_addr+(D);\
		if (b)\
			memcpy(b, p, (C));\
		read_ptr = p;\
		ret_size = (C);\
	} while (0);\
	read_ptr;\
	})

#define MWRITE(A, B, C, D)\
	({\
	void *write_ptr = NULL;\
	do {\
		UNUSED(ret_size);\
		errcode = 0;\
		ret_size = 0;\
		errno = 0;\
		size_t file_len = (D)+(C);\
		if (file_len > (A)->mmap_len) {\
			_MMAP((A), file_len);\
		} else if (file_len > (A)->mmap_file_len) {\
			int fd = fileno((A)->fptr);\
			if (fd == -1)\
				break;\
			if (fallocate(fd, 0, 0, file_len) == -1)\
				break;\
			(A)->mmap_file_len = file_len;\
		}\
		void *p = (A)->mmap_addr+(D);\
		memcpy(p, (B), (C));\
		write_ptr = p;\
		ret_size = (C);\
	} while (0);\
	if (errno)\
	{\
		ret = -1;\
		errcode = errno;\
		write_log(0, "IO error in %s. Code %d, %s, %d\n", __func__,\
			errcode, strerror(errcode), __LINE__);\
		errcode = -errcode;\
		goto errcode_handle;\
	}\
	write_ptr;\
	})

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
				if (hcfs_system->system_going_down == TRUE)\
					break;\
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
		free(ptr);                                                     \
		(ptr) = NULL;                                                  \
	} while (0)

#define ASPRINTF(...)                                                          \
	do {                                                                   \
		errno = 0;                                                     \
		ret = asprintf(__VA_ARGS__);                                   \
		if (ret == -1) {                                               \
			write_log(0, "%s: Out of memory.\n", __func__);        \
			goto errcode_handle;                                   \
		}                                                              \
	} while (0)
#endif  /* SRC_HCFS_MACRO_H_ */
