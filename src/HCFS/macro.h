/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: macro.h
* Abstract: The c header file for defining macros used in HCFS.
*
* Revision History
* 2015/5/29 Jiahong created this file.
* 2015/6/1  Kewei let macro in a block to avoid some omitted-brace 
*           situations in if-else statement.
**************************************************************************/

#ifndef GW20_HCFS_MACRO_H_
#define GW20_HCFS_MACRO_H_

#define FSEEK(A, B, C)\
	{ \
		errcode = 0; \
		ret = fseek(A, B, C); \
		if (ret < 0) { \
			errcode = errno; \
			printf("IO error in %s. Code %d, %s\n", __func__, \
					errcode, strerror(errcode)); \
			errcode = -errcode; \
			goto errcode_handle; \
		} \
	} 

#define FTELL(A)\
	{ \
		errcode = 0; \
		ret_pos = ftell(A); \
		if (ret_pos < 0) { \
			errcode = errno; \
			printf("IO error in %s. Code %d, %s\n", __func__, \
					errcode, strerror(errcode)); \
			errcode = -errcode; \
			goto errcode_handle; \
		} \
	} 

#define UNLINK(A)\
	{ \
		errcode = 0; \
		ret = unlink(A); \
		if (ret < 0) { \
			errcode = errno; \
			printf("IO error in %s. Code %d, %s\n", __func__, \
					errcode, strerror(errcode)); \
			errcode = -errcode; \
			goto errcode_handle; \
		} \
	}

#define MKDIR(A, B)\
	{ \
		errcode = 0; \
		ret = mkdir(A, B); \
		if (ret < 0) { \
			errcode = errno; \
			printf("IO error in %s. Code %d, %s\n", __func__, \
					errcode, strerror(errcode)); \
			errcode = -errcode; \
			goto errcode_handle; \
		} \
	}

#define MKNOD(A, B, C)\
	{ \
		errcode = 0; \
		ret = mknod(A, B, C); \
		if (ret < 0) { \
			errcode = errno; \
			printf("IO error in %s. Code %d, %s\n", __func__, \
					errcode, strerror(errcode)); \
			errcode = -errcode; \
			goto errcode_handle; \
		} \
	}

#define FREAD(A, B, C, D) \
	{ \
		errcode = 0; \
		ret_size = fread(A, B, C, D); \
		if ((ret < (C)) && (ferror(D) != 0)) { \
			clearerr(D); \
			printf("IO error in %s.\n", __func__);\
			errcode = -EIO; \
			goto errcode_handle; \
		} \
	}

#define FWRITE(A, B, C, D) \
	{ \
		errcode = 0; \
		ret_size = fwrite(A, B, C, D); \
		if ((ret < (C)) && (ferror(D) != 0)) { \
			clearerr(D); \
			printf("IO error in %s.\n", __func__);\
			errcode = -EIO; \
			goto errcode_handle; \
		} \
	}
#endif  /* GW20_HCFS_MACRO_H_ */
