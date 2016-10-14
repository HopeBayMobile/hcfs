/*************************************************************************

* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: alias.h
* Abstract: This c header file for case-insensitive renaming for HCFS.
*
* Revision History
* 2016/8/24 Ripley added header for this file.
*
**************************************************************************/
#ifndef SRC_HCFS_ALIAS_H_
#define SRC_HCFS_ALIAS_H_

#include <sys/stat.h>
#include <inttypes.h>

#define SEQ_INO             0           /* Tree is sorted with ino */
#define SEQ_BITMAP          1           /* Tree is sorted with bitmap */

#define MAX_NAME_MAP        4           /* (MAX_FILENAME_LEN + 1) >> 6 */
#define MAX_GROUP_LEN       64
#define MIN_ALIAS_VALUE     4611686018427387904     /* 2^62 */

#define IS_ALIAS_INODE(x)   (x > MIN_ALIAS_VALUE ? 1 : 0)
#define MAX(x, y)           (x > y ? x : y)

typedef enum {
	ALIAS = 0,
	ORDER,
	REMOVE,
	MAX_TREES,
} TREE_TYPE;

typedef enum {
	ROTATE_LL,
	ROTATE_LR,
	ROTATE_RR,
	ROTATE_RL,
} ROTATE_TYPE;

typedef enum {
	CMP_INO = 0,
	CMP_NAME_BY_BITMAP,
	CMP_INODE_BY_INO,
	CMP_INODE_BY_BITMAP,
	MAX_CMP_TYPE,
} CMP_TYPE;

typedef int (*CMP_FUNC)(const void *, const void *);

/*
 * Structure for alias inode.
 */
typedef struct ALIAS_INODE_STRUCT {
	ino_t ino;                          /* This inode number */
	uint64_t bitmap[MAX_NAME_MAP];      /* Bitmap to inode's name */
	struct ALIAS_INODE_STRUCT *left[2]; /* Left real or alias inode */
	struct ALIAS_INODE_STRUCT *right[2];/* Right real or alias inode */
	struct ALIAS_INODE_STRUCT *family;  /* Real or 1st alias in the family */
	int height[2];                      /* The inode's height in a tree */
} ALIAS_INODE, ALIAS_INODE_v1;

/*
 * Functions for handling alias inode.
 */
void init_alias_group(void);
ino_t get_real_ino_in_alias_group(ino_t this_ino);
int32_t seek_and_add_in_alias_group(ino_t real_ino, ino_t *new_inode,
	const char *real_name, const char *alias_name);
int32_t update_in_alias_group(ino_t real_ino, const char *this_name);
int32_t delete_in_alias_group(ino_t this_ino);
char *get_name_in_alias_group(ino_t real_ino, const char *this_name,
	ino_t *alias_ino);

#endif  // SRC_HCFS_ALIAS_H_
