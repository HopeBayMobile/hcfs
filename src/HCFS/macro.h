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

#ifndef GW20_HCFS_MACRO_H_
#define GW20_HCFS_MACRO_H_

#include "logger.h"

#define FSEEK(A, B, C)\
	{\
		errcode = 0;\
		ret = fseek(A, B, C);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,\
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	}

#define FTRUNCATE(A, B)\
	{\
		errcode = 0;\
		ret = ftruncate(A, B);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,\
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	}

#define SETXATTR(A, B, C, D, E)\
	{\
		errcode = 0;\
		ret = setxattr(A, B, C, D, E);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,\
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	}

#define LSEEK(A, B, C)\
	{\
		errcode = 0;\
		ret_pos = lseek(A, B, C);\
		if (ret_pos == (off_t) -1) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,\
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	}

#define FTELL(A)\
	{\
		errcode = 0;\
		ret_pos = ftell(A);\
		if (ret_pos < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,\
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	}

#define UNLINK(A)\
	{\
		errcode = 0;\
		ret = unlink(A);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,\
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	}

#define MKDIR(A, B)\
	{\
		errcode = 0;\
		ret = mkdir(A, B);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,\
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	}

#define MKNOD(A, B, C)\
	{\
		errcode = 0;\
		ret = mknod(A, B, C);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,\
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	}

#define FREAD(A, B, C, D)\
	{\
		errcode = 0;\
		ret_size = fread(A, B, C, D);\
		if ((ret_size < C) && (ferror(D) != 0)) {\
			clearerr(D);\
			write_log(0, "IO error in %s.\n", __func__);\
			errcode = -EIO;\
			goto errcode_handle;\
		} \
	}

#define FWRITE(A, B, C, D)\
	{\
		errcode = 0;\
		ret_size = fwrite(A, B, C, D);\
		if ((ret_size < C) && (ferror(D) != 0)) {\
			clearerr(D);\
			write_log(0, "IO error in %s.\n", __func__);\
			errcode = -EIO;\
			goto errcode_handle;\
		} \
	}

#define PREAD(A, B, C, D)\
	{\
		errcode = 0;\
		ret_ssize = pread(A, B, C, D);\
		if (ret_ssize < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,\
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	}

#define PWRITE(A, B, C, D)\
	{\
		errcode = 0;\
		ret_ssize = pwrite(A, B, C, D);\
		if (ret_ssize < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,\
				errcode, strerror(errcode));\
			errcode = -errcode;\
			goto errcode_handle;\
		} \
	}

#define ATOL(A)\
	{\
		errno = 0;\
		endptr = 0;\
		ret_num = strtol(A, &endptr, 10);\
		errcode = errno;\
		if (errcode != 0) {\
			write_log(0, "Conversion error in %s. Code %d, %s\n",\
				__func__, errcode, strerror(errcode));\
			goto errcode_handle;\
		} \
		if ((endptr != 0) && (*endptr != '\0')) {\
			write_log(0, "Conversion error in %s.\n",\
				__func__);\
			goto errcode_handle;\
		} \
	}

#define HTTP_PERFORM_RETRY(A)\
	{\
		num_retries = 0;\
		while (num_retries < MAX_RETRIES) {\
			res = curl_easy_perform(A);\
			if (res == CURLE_OPERATION_TIMEDOUT) {\
				num_retries++;\
				if (num_retries < MAX_RETRIES)\
					continue;\
			} else {\
				break;\
			} \
		} \
	}

#endif  /* GW20_HCFS_MACRO_H_ */
