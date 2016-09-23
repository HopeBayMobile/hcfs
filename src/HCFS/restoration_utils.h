/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: restoration_utils.h
* Abstract: The header file for restore operations
*
* Revision History
* 2016/9/22 Kewei created this file.
*
**************************************************************************/

#ifndef GW20_RESTORATION_UTILS_H_
#define GW20_RESTORATION_UTILS_H_

#include <inttypes.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>

#include "global.h"
#include "params.h"
#include "fuseop.h"

#define INCREASE_LIST_SIZE 64

typedef struct INODE_PAIR {
	ino_t src_inode;
	ino_t target_inode;
} INODE_PAIR;

typedef struct INODE_PAIR_LIST {
	INODE_PAIR *inode_pair;
	int32_t list_max_size;
	int32_t num_list_entries;
} INODE_PAIR_LIST;

/* package_uid_list */
typedef struct _PKG_LINKLIST_NODE {
	char name[MAX_FILENAME_LEN + 1];
	int32_t uid;
	struct _PKG_LINKLIST_NODE *next;
} PKG_NODE;

typedef struct {
	PKG_NODE **sarray;
	uint32_t count;
} PKG_INFO;

PKG_INFO restore_pkg_info;
PKG_NODE *pkg_info_list_head;

int32_t insert_inode_pair(INODE_PAIR_LIST *list, ino_t src_inode,
		ino_t target_inode);
int32_t find_target_inode(INODE_PAIR_LIST *list, ino_t src_inode,
		ino_t *target_inode);
INODE_PAIR_LIST *new_inode_pair_list();
void destroy_inode_pair_list(INODE_PAIR_LIST *list);

int32_t init_package_uid_list(char *xml_path);
void destroy_package_uid_list(void);
int32_t lookup_package_uid_list(const char *pkgname);

#endif
