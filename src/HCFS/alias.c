/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: alias.c
* Abstract: This c source file for some encryption helper.
*
* Revision History
* 2016/8/24 Ripley added source code for handling case-insensitive rename.
*
**************************************************************************/
#include "alias.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "global.h"
#include "logger.h"

#define DBG_LEVEL       5
#define DD1(log, ...)   write_log(DBG_LEVEL, "[AL]" log, ##__VA_ARGS__)
#define DD2(log, ...)   write_log(DBG_LEVEL, "[AL] " log, ##__VA_ARGS__)
#define DD3(log, ...)   write_log(DBG_LEVEL, "[AL]  " log, ##__VA_ARGS__)
#define DD4(log, ...)   write_log(DBG_LEVEL, "[AL]   " log, ##__VA_ARGS__)

/*
 * Global variables.
 */
ALIAS_INODE *avltree[MAX_TREES];    /* AVL trees for alias inode operation */
int64_t num_alias_inodes;           /* Reserved alias inode number */
pthread_mutex_t tree_mutex[MAX_TREES];

/*
 * Internal function proptotypes.
 */
static ino_t new_alias_inode_num(void);
static void name_to_bitmap(const char *this_name, uint64_t *bitmap);
static char *bitmap_to_name(const char *this_name, uint64_t *bitmap);

static inline void avl_lock(int tree_type)
{
	pthread_mutex_lock(&tree_mutex[tree_type]);
}
static inline void avl_unlock(int tree_type)
{
	pthread_mutex_unlock(&tree_mutex[tree_type]);
}

/*
 * Compare helper function.
 */
int compare_ino(const void *item1, const void *item2);
int compare_name_by_bitmap(const void *item1, const void *item2);
int compare_inode_by_ino(const void *item1, const void *item2);
int compare_inode_by_bitmap(const void *item1, const void *item2);

static CMP_FUNC cmp_func[MAX_CMP_TYPE] = {
	compare_ino,
	compare_name_by_bitmap,
	compare_inode_by_ino,
	compare_inode_by_bitmap,
};

/*
 * The AVL binary tree helper function.
 */
ALIAS_INODE *avl_create(ino_t this_ino, const char *this_name);
ALIAS_INODE *avl_find(ALIAS_INODE *root_entry, const void *item,
					CMP_TYPE cmp_type, int i);
ALIAS_INODE *avl_insert(ALIAS_INODE *root_entry, ALIAS_INODE *curr_entry,
					CMP_TYPE cmp_type, int i, int h);
ALIAS_INODE *avl_delete(ALIAS_INODE *root_entry, ALIAS_INODE *curr_entry,
					CMP_TYPE cmp_type, int i, int h);
ALIAS_INODE *avl_delete_all(ALIAS_INODE *root_entry);
void avl_remove_unused(ALIAS_INODE *curr_entry);
void avl_free_unused(ALIAS_INODE **root_entry, ALIAS_INODE *curr_entry,
					ino_t real_ino);


/************************************************************************
*
* Function name: init_alias_group
*        Inputs: none
*       Summary: Initial the alias variables.
*  Return value: The real_ino value, or alias_ino if not found.
*
*************************************************************************/
void init_alias_group(void)
{
	int num;

	for (num = 0; num < MAX_TREES; num++) {
		avltree[num] = NULL;
		pthread_mutex_init(&tree_mutex[num], NULL);
	}

	num_alias_inodes = 0;
}

/************************************************************************
*
* Function name: get_real_ino_in_alias_group
*        Inputs: ino_t this_ino
*       Summary: Get the real inode number in the alias group.
*  Return value: The real inode number or the original inode number.
*
*************************************************************************/
ino_t get_real_ino_in_alias_group(ino_t this_ino)
{
	ALIAS_INODE *curr_entry;

	avl_lock(REMOVE);
	curr_entry = avl_find(avltree[REMOVE], (void *)&this_ino,
						CMP_INO, SEQ_INO);
	avl_unlock(REMOVE);

	if (curr_entry == NULL) {
		avl_lock(ORDER);
		curr_entry = avl_find(avltree[ORDER], (void *)&this_ino,
							CMP_INO, SEQ_INO);
		avl_unlock(ORDER);

		if (curr_entry == NULL)
			return this_ino;
	}

	/* Return the real inode number. */
	return curr_entry->family->ino;
}

/************************************************************************
*
* Function name: seek_and_add_in_alias_group
*        Inputs: ino_t real_ino, ino_t *new_inode, const char *real_name,
*                const char *alias_name
*       Summary: Search the alias inode and return the new alias inode number.
*                If not found, create it in the alias avltrees and dynamic
*                assigned the　inode number to the new alias inode.
*  Return value: Return 0 if found or created successfully, and *new_inode
*                value represent the alias inode number.
*
*************************************************************************/
int32_t seek_and_add_in_alias_group(ino_t real_ino, ino_t *new_inode,
	const char *real_name, const char *alias_name)
{
	ALIAS_INODE *parent, *child;
	ino_t alias_ino;

	if (!new_inode || !real_name || !alias_name)
		return -EINVAL;

	avl_lock(ALIAS);

	DD1("+SEEK: (real, alias) = (%s, %s)\n", real_name, alias_name);

	/* Find the real inode position. Create it if not found. */
	parent = avl_find(avltree[ALIAS], (void *)&real_ino,
					CMP_INO, SEQ_INO);
	if (parent == NULL) {
		avl_unlock(ALIAS);
		/* Create the root alias inode. */
		DD2("Create real inode('%s', %" PRIu64 ")\n", real_name, real_ino);
		parent = avl_create(real_ino, real_name);
		if (parent == NULL)
			return -ENOMEM;

		/* Add this root alias inode to avltree[ALIAS] */
		avl_lock(ALIAS);
		DD2("Insert real inode to tree(A)\n");
		avltree[ALIAS] = avl_insert(avltree[ALIAS], parent,
									CMP_INODE_BY_INO, SEQ_INO, 0);
	}

	/* Find the alias inode position. Create it if not found. */
	child = avl_find(parent->family, (void *)alias_name,
					CMP_NAME_BY_BITMAP, SEQ_BITMAP);
	if (child == NULL) {
		avl_unlock(ALIAS);
		/* Given a virtual alias inode number. */
		alias_ino = new_alias_inode_num();
		/* Create the alias inode. */
		DD2("Create alias inode('%s', %" PRIu64 ")\n", alias_name, alias_ino);
		child = avl_create(alias_ino, alias_name);
		if (child == NULL) {
			free(parent);
			return -ENOMEM;
		}

		/* Add the child to the parent->family avltree. */
		avl_lock(ALIAS);
		DD2("Insert alias inode to tree(I)\n");
		parent->family = avl_insert(parent->family, child,
									CMP_INODE_BY_BITMAP, SEQ_BITMAP, 0);
		child->family = parent;

		/* Add the child to the avltree[ORDER]. */
		avl_lock(ORDER);
		DD2("Insert alias inode to tree(O)\n");
		avltree[ORDER] = avl_insert(avltree[ORDER], child,
									CMP_INODE_BY_INO, SEQ_INO, 0);
		avl_unlock(ORDER);
	}

	*new_inode = child->ino;

	DD1("-SEEK: Done(%" PRIu64 ")\n", child->ino);
	avl_unlock(ALIAS);

	return 0;
}

/************************************************************************
*
* Function name: update_in_alias_group
*        Inputs: ino_t real_ino, const char *this_name
*       Summary: Update the real inode's bitmap to new value generated from
*                this_name. Remove this alias inode from the avltree[ALIAS]
*                and move it from the avltree[ORDER] to the avltree[REMOVE].
*  Return value: Return 0 if updated successfully.
*
*************************************************************************/
int32_t update_in_alias_group(ino_t real_ino, const char *this_name)
{
	ALIAS_INODE *parent, *child;

	if (!this_name)
		return -EINVAL;

	avl_lock(ALIAS);

	DD1("+UPDATE: Real inode %" PRIu64 " name to '%s'\n", real_ino, this_name);

	/* Find the real inode position. */
	parent = avl_find(avltree[ALIAS], (void *)&real_ino,
					CMP_INO, SEQ_INO);
	if (parent == NULL) {
		DD1("-UPDATE: Done(Not found %" PRIu64 ")\n", real_ino);
		avl_unlock(ALIAS);
		return -ENOENT;
	}

	/* Update root alias inode's bitmap to new value. */
	name_to_bitmap(this_name, parent->bitmap);

	/* Find the alias inode position. */
	child = avl_find(parent->family, (void *)this_name,
					CMP_NAME_BY_BITMAP, SEQ_BITMAP);
	if (child == NULL) {
		/*
		 * If the alias inode is not found, that means the real inode's name
		 * is changed to another different name instead of case-insensitive.
		 * In this case, we must delete all alias inodes for this real inode.
		 */
		DD2("Remove all alias inodes from tree(I)\n");
		parent->family = avl_delete_all(parent->family);

		/* Free all alias inodes in the avltree[REMOVE]. */
		avl_lock(REMOVE);
		avl_free_unused(&avltree[REMOVE], avltree[REMOVE], real_ino);
		avl_unlock(REMOVE);

		DD1("-UPDATE: Done(%" PRIu64 ")\n", real_ino);
		avl_unlock(ALIAS);
		return 0;
	}

	/* Remove the child from the parent->family avltree. */
	DD2("Remove alias inode from tree(I)\n");
	parent->family = avl_delete(parent->family, child,
								CMP_INODE_BY_BITMAP, SEQ_BITMAP, 0);
	avl_remove_unused(child);

	DD1("-UPDATE: Done(%" PRIu64 ")\n", real_ino);
	avl_unlock(ALIAS);

	return 0;
}

/************************************************************************
*
* Function name: delete_in_alias_group
*        Inputs: ino_t this_ino
*       Summary: Delete and free the real/alias inode in the alias group.
*  Return value: Return 0 if deleted successfully.
*
*************************************************************************/
int32_t delete_in_alias_group(ino_t this_ino)
{
	ALIAS_INODE *parent, *child;
	ino_t real_ino = this_ino;

	if (IS_ALIAS_INODE(this_ino))
		real_ino = get_real_ino_in_alias_group(this_ino);

	avl_lock(ALIAS);

	/* Find the real inode position. */
	parent = avl_find(avltree[ALIAS], (void *)&real_ino,
					CMP_INO, SEQ_INO);
	if (parent == NULL) {
		avl_unlock(ALIAS);
		return -ENOENT;
	}

	DD1("+DELETE: Inode %" PRIu64 "\n", this_ino);

	if (IS_ALIAS_INODE(this_ino)) {
		/*
		 * Check if the alias inode is existed in the avltree[ORDER]. If the
		 * inode is found, move it to avltree[REMOVE] for later deletion.
		 * If not found, it means the inode is either in the avltree[REMOVE]
		 * or already removed. In this case, skip this operation.
		 */
		avl_lock(ORDER);
		child = avl_find(avltree[ORDER], (void *)&this_ino,
						CMP_INO, SEQ_INO);
		avl_unlock(ORDER);
		if (child != NULL) {
			/* Remove the child from the parent->family avltree. */
			DD2("Remove alias inode from tree(I)\n");
			parent->family = avl_delete(parent->family, child,
										CMP_INODE_BY_BITMAP, SEQ_BITMAP, 0);
			avl_remove_unused(child);
		} else {
			DD1("-DELETE: Done(SKIP)\n");
			avl_unlock(ALIAS);
			return 0;
		}
	} else {
		/* Delete all alias inodes in the alias group. */
		DD2("Remove all alias inodes from tree(I)\n");
		parent->family = avl_delete_all(parent->family);

		/* Free all alias inodes in the avltree[REMOVE]. */
		avl_lock(REMOVE);
		avl_free_unused(&avltree[REMOVE], avltree[REMOVE], this_ino);
		avl_unlock(REMOVE);

		/* Remove the parent from the avltree[ALIAS]. */
		DD2("Remove real inode from tree(A)\n");
		avltree[ALIAS] = avl_delete(avltree[ALIAS], parent,
									CMP_INODE_BY_INO, SEQ_INO, 0);
		/* Free the parent inode. */
		DD2("Free real inode %" PRIu64 "\n", this_ino);
		free(parent);
	}

	DD1("-DELETE: Done(%" PRIu64 ")\n", this_ino);
	avl_unlock(ALIAS);

	return 0;
}

/************************************************************************
*
* Function name: get_alias_in_alias_group
*        Inputs: ino_t real_ino, const char *this_name, ino_t *alias_ino
*       Summary: Get the available alias inode from the root of the tree.
*                Return it's actual name and it's inode number.
*  Return value: Return alias inode's name, or NULL if not found.
*
*************************************************************************/
char *get_alias_in_alias_group(ino_t real_ino, const char *this_name,
	ino_t *alias_ino)
{
	ALIAS_INODE *parent, *child;
	char *alias_name;

	avl_lock(ALIAS);

	DD1("+GETNAME: Inode %" PRIu64 " lowercase '%s'\n", real_ino, this_name);

	/* Find the real inode position. */
	parent = avl_find(avltree[ALIAS], (void *)&real_ino,
					CMP_INO, SEQ_INO);
	if (parent == NULL || ((child = parent->family) == NULL)) {
		DD1("-GETNAME: Done(END)\n");
		avl_unlock(ALIAS);
		return NULL;
	}

	/* Remove the child from the parent->family avltree. */
	DD2("Remove alias inode from tree(I)\n");
	parent->family = avl_delete(parent->family, child,
								CMP_INODE_BY_BITMAP, SEQ_BITMAP, 0);
	avl_remove_unused(child);

	*alias_ino = child->ino;
	alias_name = bitmap_to_name(this_name, child->bitmap);

	DD1("-GETNAME: Done('%s')\n", alias_name);
	avl_unlock(ALIAS);

	return alias_name;
}


/*
 * Assign the alias inode a new inode number. Starting from 2^62+1.
 */
static ino_t new_alias_inode_num(void)
{
	num_alias_inodes++;
	return (ino_t)(MIN_ALIAS_VALUE + num_alias_inodes);
}

/*
 * Transfer an inode's name into a bitmap value.
 */
static void name_to_bitmap(const char *this_name, uint64_t *bitmap)
{
	int32_t length, index = 0, group;

	memset(bitmap, 0, sizeof(uint64_t) * MAX_NAME_MAP);
	length = strlen(this_name);

	while (index < length) {
		group = index / MAX_GROUP_LEN;
		if ((this_name[index] >= 'A') && (this_name[index] <= 'Z')) {
			bitmap[group] |= 1;
		}
		index = index + 1;
		if ((index < length) && (index % MAX_GROUP_LEN > 0))
			bitmap[group] <<= 1;
	}
}

/*
 * Restore an inode's name from a bitmap value.
 */
static char *bitmap_to_name(const char *this_name, uint64_t *bitmap)
{
	int32_t length, index = 0, group, remain;
	int32_t i, j;
	char *actual_name;

	length = strlen(this_name);
	actual_name = malloc(length + 1);
	if (actual_name == NULL) {
		DD2("No memory!\n");
		return NULL;
	}

	group = length / MAX_GROUP_LEN;
	remain = length % MAX_GROUP_LEN;

	for (i = 0; i < group; i++) {
		for (j = MAX_GROUP_LEN; j > 0; j--, index++) {
			actual_name[index] = this_name[index];
			if (bitmap[i] & (1ull << (j - 1)))
				actual_name[index] -= 32;
		}
	}

	for (j = remain; j > 0; j--, index++) {
		actual_name[index] = this_name[index];
		if (bitmap[i] & (1ull << (j - 1)))
			actual_name[index] -= 32;
	}

	actual_name[index] = '\0';

	return actual_name;
}


/*
 * Compare helper function.
 */
int compare_ino(const void *item1, const void *item2)
{
	const ino_t *a = (const ino_t *)item1;
	const ALIAS_INODE *b = (const ALIAS_INODE *)item2;

	if (*a < b->ino)
		return -1;
	else if (*a > b->ino)
		return 1;
	else
		return 0;
}

int compare_name_by_bitmap(const void *item1, const void *item2)
{
	const char *name = (const char *)item1;
	uint64_t a[MAX_NAME_MAP];
	const ALIAS_INODE *b = (const ALIAS_INODE *)item2;

	name_to_bitmap(name, a);

	return memcmp(a, b->bitmap, sizeof(uint64_t) * MAX_NAME_MAP);
}

int compare_inode_by_ino(const void *item1, const void *item2)
{
	const ALIAS_INODE *a = (const ALIAS_INODE *)item1;
	const ALIAS_INODE *b = (const ALIAS_INODE *)item2;

	if (a->ino < b->ino)
		return -1;
	else if (a->ino > b->ino)
		return 1;
	else
		return 0;
}

int compare_inode_by_bitmap(const void *item1, const void *item2)
{
	const ALIAS_INODE *a = (const ALIAS_INODE *)item1;
	const ALIAS_INODE *b = (const ALIAS_INODE *)item2;

	return memcmp(a->bitmap, b->bitmap, sizeof(uint64_t) * MAX_NAME_MAP);
}

/*
 * The AVL binary tree helper function.
 */
static int height(ALIAS_INODE *curr_entry, int i)
{
	return (curr_entry ? curr_entry->height[i] : 0);
}

static void update_height(ALIAS_INODE *curr_entry, int i)
{
	curr_entry->height[i] = MAX(height(curr_entry->left[i], i),
							height(curr_entry->right[i], i)) + 1;
}

static int balanced_factor(ALIAS_INODE *curr_entry, int i)
{
	if (curr_entry == NULL)
		return 0;

	return height(curr_entry->left[i], i) - height(curr_entry->right[i], i);
}

static ALIAS_INODE *get_min_alias_inode(ALIAS_INODE *root_entry, int i)
{
	ALIAS_INODE* curr_entry = root_entry;

	/* The minimum alias inode is at the left most */
	while (curr_entry->left[i] != NULL)
		curr_entry = curr_entry->left[i];

	return curr_entry;
}

static void reset_entry(ALIAS_INODE *curr_entry, int i)
{
	DD4("Reset inode %" PRIu64 "(%d)\n", curr_entry->ino, i);
	curr_entry->height[i] = 1;
	curr_entry->left[i] = NULL;
	curr_entry->right[i] = NULL;
}

ALIAS_INODE *avl_create(ino_t this_ino, const char *this_name)
{
	ALIAS_INODE *new_entry;

	new_entry = malloc(sizeof(ALIAS_INODE));
	if (!new_entry) {
		DD3("No memory!\n");
		return NULL;
	}

	/* Fill data. */
	name_to_bitmap(this_name, new_entry->bitmap);
	new_entry->ino = this_ino;
	new_entry->family = NULL;
	reset_entry(new_entry, 0);
	reset_entry(new_entry, 1);

	return new_entry;
}

ALIAS_INODE *avl_find(ALIAS_INODE *root_entry, const void *item,
	CMP_TYPE cmp_type, int i)
{
	ALIAS_INODE *curr_entry = root_entry;
	int cmp_result = 0;

	if (root_entry == NULL)
		return NULL;

	while (curr_entry) {
		cmp_result = cmp_func[cmp_type](item, (void *)curr_entry);
		if (cmp_result == 0)
			return curr_entry;

		if (cmp_result < 0)
			curr_entry = curr_entry->left[i];
		else
			curr_entry = curr_entry->right[i];
	}
	/* Not found */
	return NULL;
}

ALIAS_INODE *avl_left_rotate(ALIAS_INODE *curr_entry, int i)
{
	ALIAS_INODE *old_root = curr_entry;
	ALIAS_INODE *new_root = curr_entry->right[i];

	/* rotate */
	old_root->right[i] = new_root->left[i];
	new_root->left[i] = old_root;

	/* update height */
	update_height(old_root, i);
	update_height(new_root, i);

	return new_root;
}

ALIAS_INODE *avl_right_rotate(ALIAS_INODE *curr_entry, int i)
{
	ALIAS_INODE *old_root = curr_entry;
	ALIAS_INODE *new_root = curr_entry->left[i];

	/* rotate */
	old_root->left[i] = new_root->right[i];
	new_root->right[i] = old_root;

	/* update height */
	update_height(old_root, i);
	update_height(new_root, i);

	return new_root;
}

ALIAS_INODE *avl_balance(ALIAS_INODE *curr_entry, ROTATE_TYPE rt_type, int i)
{
	switch (rt_type) {
	case ROTATE_LL:
		DD3("LL rotation on inode %" PRIu64 "\n", curr_entry->ino);
		curr_entry = avl_right_rotate(curr_entry, i);
		break;
	case ROTATE_LR:
		DD3("LR rotation on inode %" PRIu64 "\n", curr_entry->ino);
		curr_entry->left[i] = avl_left_rotate(curr_entry->left[i], i);
		curr_entry = avl_right_rotate(curr_entry, i);
		break;
	case ROTATE_RR:
		DD3("RR rotation on inode %" PRIu64 "\n", curr_entry->ino);
		curr_entry = avl_left_rotate(curr_entry, i);
		break;
	case ROTATE_RL:
		DD3("RL rotation on inode %" PRIu64 "\n", curr_entry->ino);
		curr_entry->right[i] = avl_right_rotate(curr_entry->right[i], i);
		curr_entry = avl_left_rotate(curr_entry, i);
		break;
	default:
		break;
	}

	return curr_entry;
}

ALIAS_INODE *avl_insert(ALIAS_INODE *root_entry, ALIAS_INODE *curr_entry,
	CMP_TYPE cmp_type, int i, int h)
{
	int cmp_result, level;
	ROTATE_TYPE rotate_type = ROTATE_NONE;

	if (root_entry == NULL) {
		DD3("Insert %s inode %" PRIu64 "(H:%d)\n", h ? "leaf" : "root",
			curr_entry->ino, h);
		return curr_entry;
	}

	/* Compare the new inode with the inode in the tree. */
	cmp_result = cmp_func[cmp_type]((void *)curr_entry, (void *)root_entry);

	/* Insert the new inode to the tree. */
	if (cmp_result < 0) {
		DD3("Inode %" PRIu64 "(H:%d) L%d ptr\n", root_entry->ino, h, i);
		root_entry->left[i] = avl_insert(root_entry->left[i], curr_entry,
										cmp_type, i, h+1);
	} else {
		DD3("Inode %" PRIu64 "(H:%d) R%d ptr\n", root_entry->ino, h, i);
		root_entry->right[i] = avl_insert(root_entry->right[i], curr_entry,
										cmp_type, i, h+1);
	}

	/* Update the current inode's height. */
	update_height(root_entry, i);

	/* Get the balanced factor to check if it's an unbalanced tree. */
	level = balanced_factor(root_entry, i);

	/* Unbalanced condition: LL or LR case. */
	if (level > 1) {
		cmp_result = cmp_func[cmp_type]((void *)curr_entry,
										(void *)root_entry->left[i]);
		if (cmp_result < 0)
			rotate_type = ROTATE_LL;
		else
			rotate_type = ROTATE_LR;
	/* Unbalanced condition: RR or RL case. */
	} else if (level < -1) {
		cmp_result = cmp_func[cmp_type]((void *)curr_entry,
										(void *)root_entry->right[i]);
		if (cmp_result > 0)
			rotate_type = ROTATE_RR;
		else
			rotate_type = ROTATE_RL;
	}

	if (rotate_type != ROTATE_NONE)
		root_entry = avl_balance(root_entry, rotate_type, i);

	DD3("Link to inode %" PRIu64 "(H:%d)\n", root_entry->ino, h);
	return root_entry;
}

ALIAS_INODE *avl_delete(ALIAS_INODE *root_entry, ALIAS_INODE *curr_entry,
	CMP_TYPE cmp_type, int i, int h)
{
	int cmp_result, level;
	ALIAS_INODE *child;
	ROTATE_TYPE rotate_type = ROTATE_NONE;

	if (root_entry == NULL)
		return NULL;

	if (h == 0)
		DD3("Process delete inode %"PRIu64 "\n", curr_entry->ino);

	/* Compare the new inode with the current one. */
	cmp_result = cmp_func[cmp_type]((void *)curr_entry, (void *)root_entry);

	/* The inode is at the left subtree, keep finding it until reach it. */
	if (cmp_result < 0) {
		DD3("Inode %" PRIu64 "(H:%d) L%d ptr\n", root_entry->ino, h, i);
		root_entry->left[i] = avl_delete(root_entry->left[i], curr_entry,
										cmp_type, i, h+1);
		reset_entry(curr_entry, i);
	/* The inode is at the right subtree, keep finding it until reach it. */
	} else if (cmp_result > 0) {
		DD3("Inode %" PRIu64 "(H:%d) R%d ptr\n", root_entry->ino, h, i);
		root_entry->right[i] = avl_delete(root_entry->right[i], curr_entry,
										cmp_type, i, h+1);
		reset_entry(curr_entry, i);
	/* Find the inode, perform the deletion. */
	} else {
		if ((root_entry->left[i] == NULL) || (root_entry->right[i] == NULL)) {
			if (root_entry->left[i] == NULL) {
				child = root_entry->right[i];
			} else {
				child = root_entry->left[i];
			}

			/* No child. */
			if (child == NULL) {
				DD3("Remove inode %" PRIu64 "(H:%d)\n", root_entry->ino, h);
				root_entry = NULL;
			/* One child. */
			} else {
				DD3("Replace inode %" PRIu64 " with inode %" PRIu64 "(H:%d)\n",
						root_entry->ino, child->ino, h);
				reset_entry(root_entry, i);
				root_entry = child;
			}
		} else {
			/* Two children. */
			/* Get the minimum alias inode from the right subtree. */
			child = get_min_alias_inode(root_entry->right[i], i);

			DD3("Replace inode %" PRIu64 " with inode %" PRIu64 "(H:%d)\n",
					root_entry->ino, child->ino, h);

			/* Delete the original child. */
			root_entry->right[i] = avl_delete(root_entry->right[i], child,
											cmp_type, i, h+1);

			/* Replace the root_entry with the child. */
			child->left[i] = root_entry->left[i];
			child->right[i] = root_entry->right[i];
			reset_entry(root_entry, i);
			root_entry = child;
			DD3("Replace done(H:%d)\n", h);
		}
	}

	if (root_entry == NULL)
		return NULL;

	/* Update the current inode's height. */
	update_height(root_entry, i);

	/* Get the balanced factor to check if it's an unbalanced tree. */
	level = balanced_factor(root_entry, i);

	/* Unbalanced condition: LL or LR case. */
	if (level > 1) {
		if (balanced_factor(root_entry->left[i], i) > 0)
			rotate_type = ROTATE_LL;
		else
			rotate_type = ROTATE_LR;
	/* Unbalanced condition: RR or RL case. */
	} else if (level < -1) {
		if (balanced_factor(root_entry->right[i], i) < 0)
			rotate_type = ROTATE_RR;
		else
			rotate_type = ROTATE_RL;
	}

	if (rotate_type != ROTATE_NONE)
		root_entry = avl_balance(root_entry, rotate_type, i);

	DD3("Link to inode %" PRIu64 "(H:%d)\n", root_entry->ino, h);
	return root_entry;
}

ALIAS_INODE *avl_delete_all(ALIAS_INODE *root_entry)
{
	if (root_entry == NULL)
		return NULL;

	DD2("Delete alias inode %" PRIu64 "\n", root_entry->ino);

	/* Remove all left subtrees. */
	root_entry->left[SEQ_BITMAP] =
				avl_delete_all(root_entry->left[SEQ_BITMAP]);

	/* Remove all right subtrees. */
	root_entry->right[SEQ_BITMAP] =
				avl_delete_all(root_entry->right[SEQ_BITMAP]);

	/* Remove the inode in the avltree[ORDER]. */
	avl_lock(ORDER);
	DD2("Remove alias inode from tree(O)\n");
	avltree[ORDER] = avl_delete(avltree[ORDER], root_entry,
								CMP_INODE_BY_INO, SEQ_INO, 0);
	avl_unlock(ORDER);

	/* Free the inode. */
	DD2("Free alias inode %" PRIu64 "\n", root_entry->ino);
	free(root_entry);

	return NULL;
}

void avl_remove_unused(ALIAS_INODE *curr_entry)
{
	/* Remove the inode in the avltree[ORDER]. */
	avl_lock(ORDER);
	DD2("Remove alias inode from tree(O)\n");
	avltree[ORDER] = avl_delete(avltree[ORDER], curr_entry,
								CMP_INODE_BY_INO, SEQ_INO, 0);
	avl_unlock(ORDER);

	/* Insert the child to the avltree[REMOVE]. */
	avl_lock(REMOVE);
	DD2("Insert alias inode to tree(R)\n");
	avltree[REMOVE] = avl_insert(avltree[REMOVE], curr_entry,
								CMP_INODE_BY_INO, SEQ_INO, 0);
	avl_unlock(REMOVE);
}

void avl_free_unused(ALIAS_INODE **root_entry, ALIAS_INODE *curr_entry,
	ino_t real_ino)
{
	if (curr_entry == NULL)
		return;

	/* Search left subtrees and remove the real inode's family. */
	avl_free_unused(&curr_entry->left[SEQ_INO],
					curr_entry->left[SEQ_INO], real_ino);

	/* Search right subtrees and remove the real inode's family. */
	avl_free_unused(&curr_entry->right[SEQ_INO],
					curr_entry->right[SEQ_INO], real_ino);

	/* Delete and free the inode if it's the real inode's family. */
	if (curr_entry->family->ino == real_ino) {
		DD2("Remove alias inode from tree(R)\n");
		*root_entry = avl_delete(*root_entry, curr_entry,
								CMP_INODE_BY_INO, SEQ_INO, 0);
		DD2("Free alias inode %" PRIu64 "\n", curr_entry->ino);
		free(curr_entry);
	}
}
