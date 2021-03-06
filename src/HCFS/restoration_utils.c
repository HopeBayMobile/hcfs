/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "restoration_utils.h"

#include <errno.h>
#include <string.h>
//TODO: temp remove #include <ftw.h>
#include <regex.h>
#include <stddef.h>

#include "metaops.h"
#include "file_present.h"
#include "do_restoration.h"
#include "control_smartcache.h"

/**
 * Given a path, try to find stat in now mounted HCFS.
 */
int32_t stat_device_path(char *path, HCFS_STAT *hcfsstat)
{
	char *path_ptr, *rest_ptr, *token;
	int32_t ret;
	ino_t now_ino;
	DIR_ENTRY dentry;

	if (strncmp(path, "/data/data", strlen("/data/data")) != 0) {
		write_log(4, "Warn: Try to stat path %s in %s",
				path, __func__);
		ret = -EINVAL;
		goto out;
	}

	now_ino = data_data_root;
	path_ptr = path + strlen("/data/data");
	token = strtok_r(path_ptr, "/", &rest_ptr);
	while (token) {
		ret = lookup_dir(now_ino, token, &dentry, FALSE);
		if (ret < 0)
			goto out;
		now_ino = dentry.d_ino;
		token = strtok_r(rest_ptr, "/", &rest_ptr);
	}

	ret = fetch_inode_stat(now_ino, hcfsstat, NULL, NULL);

out:
	return ret;
}

int32_t _inode_bsearch(INODE_PAIR_LIST *list, ino_t src_inode, int32_t *index)
{
	int32_t start_index, end_index, mid_index;
	int64_t cmp_result;

	start_index = 0;
	end_index = list->num_list_entries;
	mid_index = (end_index + start_index) / 2;

	while (end_index > start_index) {

		if (mid_index >= list->list_max_size) {
			mid_index = -1; /* Not found and list is full */
			break;
		}

		if (mid_index >= list->num_list_entries)
			break;

		cmp_result = src_inode - list->inode_pair[mid_index].src_inode;
		if (cmp_result == 0) {
			*index = mid_index;
			return 0;
		} else if (cmp_result < 0) {
			end_index = mid_index;
		} else {
			start_index = mid_index + 1;
		}
		mid_index = (end_index + start_index) / 2;
	}

	/* Key entry not found */
	*index = mid_index;
	return -ENOENT;
}

/**
 * Given pair (src_inode, target_inode), insert them to the sorted list.
 *
 * @param list Structure of inode pair list.
 * @param src_inode Source inode number corresponding to now system
 * @param target_inode Target inode number corresponding to restored system.
 *
 * @return 0 on insertion success, otherwise negation of error code.
 */
int32_t insert_inode_pair(INODE_PAIR_LIST *list, ino_t src_inode,
		ino_t target_inode)
{
	int32_t ret;
	int32_t index;

	ret = _inode_bsearch(list, src_inode, &index);
	if (ret == 0)
		return 0;
	if (index < 0)
		return -EFAULT;

	memmove(list->inode_pair + index + 1, list->inode_pair + index,
		(list->num_list_entries - index + 1) * sizeof(INODE_PAIR));
	list->inode_pair[index].src_inode = src_inode;
	list->inode_pair[index].target_inode = target_inode;
	list->num_list_entries += 1;
	if (list->num_list_entries >= list->list_max_size - 1) {
		list->list_max_size += INCREASE_LIST_SIZE;
		list->inode_pair = realloc(list->inode_pair,
			sizeof(INODE_PAIR) * list->list_max_size);
		if (list->inode_pair == NULL)
			return -ENOMEM;
	}

	return 0;
}

/**
 * Given source inode number "src_inode", find the target inode number in list.
 *
 * @param list Structure of inode pair list.
 * @param src_inode Source inode number corresponding to now system
 * @param target_inode Target inode number corresponding to restored system.
 *
 * @return 0 on success, otherwise -ENOENT indicating src_inode not found.
 */
int32_t find_target_inode(INODE_PAIR_LIST *list, ino_t src_inode,
		ino_t *target_inode)
{
	int32_t ret;
	int32_t index;

	ret = _inode_bsearch(list, src_inode, &index);
	if (ret < 0)
		return ret;

	*target_inode = list->inode_pair[index].target_inode;
	return 0;
}

/**
 * Allocate a data structure INODE_PAIR_LIST and return the address.
 *
 * @return Pointer of a INODE_PAIR_LIST list.
 */
INODE_PAIR_LIST *new_inode_pair_list()
{
	INODE_PAIR_LIST *list;

	list = (INODE_PAIR_LIST *)calloc(sizeof(INODE_PAIR_LIST), 1);
	if (!list) {
		write_log(0, "Error: Fail to alloc memory in %s.", __func__);
		return NULL;
	}

	list->list_max_size = INCREASE_LIST_SIZE;
	list->inode_pair = (INODE_PAIR *) calloc(sizeof(INODE_PAIR),
			list->list_max_size);
	if (!(list->inode_pair)) {
		write_log(0, "Error: Fail to alloc memory in %s.", __func__);
		FREE(list);
		return NULL;
	}

	return list;
}

/**
 * Free all memory resource of the list.
 *
 * @return none.
 */
void destroy_inode_pair_list(INODE_PAIR_LIST *list)
{
	FREE(list->inode_pair);
	FREE(list);
}

int _compare_pkg_entry_ptr(const void *a, const void *b)
{
	PKG_NODE *A = *(PKG_NODE **)a, *B = *(PKG_NODE **)b;

	return strcmp(A->name, B->name);
}

/*
 * Read packages xml to build a sorted array for uid lookup
 *
 * 1. Parse package.xml and build a link list.
 * 2. fill list into an array of pointer.
 * 3. qsort the array for bsearch later.
 *
 * @return 0 on success, -errno on error.
 */
int32_t init_package_uid_list(char *plistpath)
{
	FILE *src = NULL;
	int32_t errcode = 0;
	char fbuf[4100], *sptr;
	regex_t re = {.re_nsub = 0};
	regmatch_t pm[10];
	const size_t nmatch = 10;
	char ebuff[1024];
	PKG_NODE *last_node = NULL, *tmp_pkg = NULL, *tmp_cur;
	size_t pkg_cnt = 0;
	size_t i;

	if (pkg_info_list_head)
		destroy_package_uid_list();

#define namepat " name=\"([^\"]+)\""
#define uidpat " (sharedUserId|userId)=\"([[:digit:]]+)\""
	errcode =
	    regcomp(&re, "^[[:space:]]*<package.*" namepat ".*" uidpat ".*$",
		    REG_EXTENDED | REG_NEWLINE);
	if (errcode != 0) {
		regerror(errcode, &re, ebuff, 1024);
		errcode = -errcode;
		write_log(0, "%s: Error when compiling regex pattern. (%s)\n",
			  __func__, ebuff);
		goto errcode_handle;
	}

	src = fopen(plistpath, "r");
	if (src == NULL) {
		errcode = -errno;
		write_log(0, "%s: Error when opening %s. (%s)\n", __func__,
			  plistpath, strerror(-errcode));
		goto errcode_handle;
	}
	clearerr(src);
	while (!feof(src)) {
		sptr = fgets(fbuf, 4096, src);
		if (sptr == NULL)
			break;
		errcode = regexec(&re, sptr, nmatch, pm, 0);
		if (errcode != 0) { /* not match */
			errcode = 0;
			continue;
		}
		pkg_cnt++;
		write_log(10, "%s [%zu]: %.*s %.*s\n", __func__, pkg_cnt,
			  (pm[1].rm_eo - pm[1].rm_so), &sptr[pm[1].rm_so],
			  (pm[3].rm_eo - pm[3].rm_so), &sptr[pm[3].rm_so]);

		tmp_pkg = (PKG_NODE *)calloc(1, sizeof(PKG_NODE));
		if (tmp_pkg == NULL) {
			errcode = -errno;
			goto errcode_handle;
		}
		/* Append package node to list */
		if (pkg_info_list_head == NULL)
			pkg_info_list_head = tmp_pkg;

		/* Update last node */
		if (last_node && last_node != tmp_pkg)
			last_node->next = tmp_pkg;
		last_node = tmp_pkg;

		/* Set name */
		strncpy(tmp_pkg->name, &sptr[pm[1].rm_so],
			pm[1].rm_eo - pm[1].rm_so);
		tmp_pkg->name[pm[1].rm_eo - pm[1].rm_so] = '\0';
		/* Set uid */
		sptr[pm[3].rm_eo] = '\0';
		tmp_pkg->uid = ATOL(&sptr[pm[3].rm_so]);

	}
	if (ferror(src) && !feof(src)) {
		write_log(0, "%s: Error when reading %s.\n", __func__,
			  plistpath);
		errcode = -ferror(src);
		goto errcode_handle;
	}

	if (!pkg_cnt) {
		write_log(0, "%s: Malformed content retrieved from %s.\n",
			  __func__, plistpath);
		errcode = -EINVAL;
		goto errcode_handle;
	}

	/* Build & sort array */
	restore_pkg_info.sarray =
	    (PKG_NODE **)malloc(pkg_cnt * sizeof(PKG_NODE *));
	if (restore_pkg_info.sarray == NULL) {
		errcode = -errno;
		goto errcode_handle;
	}
	restore_pkg_info.count = pkg_cnt;


	i = 0;
	tmp_cur = pkg_info_list_head;
	while (i < pkg_cnt && tmp_cur != NULL) {
		restore_pkg_info.sarray[i] = tmp_cur;
		i++;
		tmp_cur = tmp_cur->next;
	}
	qsort(restore_pkg_info.sarray, pkg_cnt, sizeof(PKG_NODE **),
	      _compare_pkg_entry_ptr);

errcode_handle:
	regfree(&re);
	if (src != NULL)
		fclose(src);
	if (errcode != 0)
		destroy_package_uid_list();

	return errcode;
}

void destroy_package_uid_list(void)
{
	PKG_NODE *tmp_cur, *tmp_next;

	tmp_cur = pkg_info_list_head;
	while (tmp_cur != NULL) {
		tmp_next = tmp_cur->next;
		free(tmp_cur);
		tmp_cur = tmp_next;
	}
	free(restore_pkg_info.sarray);

	pkg_info_list_head = NULL;
	restore_pkg_info.sarray = NULL;
}

/*
 * lookup package uid bt name
 *
 * @return uid if found entry, -1 if not found, -errno on other error
 */
int32_t lookup_package_uid_list(const char *pkgname)
{
	PKG_NODE *key = (PKG_NODE *)malloc(sizeof(PKG_NODE)), **ret;

	if (key == NULL)
		return -errno;

	strncpy(key->name, pkgname, sizeof(key->name));
	ret = bsearch(&key, restore_pkg_info.sarray,
				   restore_pkg_info.count, sizeof(PKG_NODE **),
				   _compare_pkg_entry_ptr);
	free(key);
	return (ret != NULL) ? (*ret)->uid : -1;
}

int32_t create_smartcache_symlink(ino_t this_inode, ino_t root_ino,
		char *pkgname)
{
	char metapath[METAPATHLEN];
	char link_target[MAX_LINK_PATH];
	SYMLINK_META_HEADER symlink_header;
	struct stat tmpstat;
	int64_t uid, metasize, metasize_blk;
	FILE *fptr;

	uid = lookup_package_uid_list(pkgname);
	if (uid < 0)
		return -ENOENT;
	memset(&symlink_header, 0, sizeof(SYMLINK_META_HEADER));
	sprintf(link_target, "%s/%s", SMART_CACHE_MP, pkgname);
	init_hcfs_stat(&(symlink_header.st));
	symlink_header.st.ino = this_inode;
	symlink_header.st.mode = S_IFLNK;
	symlink_header.st.uid = uid;
	symlink_header.st.gid = uid;
	symlink_header.smt.link_len = strlen(link_target);
	symlink_header.smt.generation = 1;
	symlink_header.smt.source_arch = ARCH_CODE;
	symlink_header.smt.root_inode = root_ino;
	symlink_header.smt.local_pin = P_PIN;
	strncpy(symlink_header.smt.link_path, link_target, strlen(link_target));
	/* cloud related data are all zero. */
	/* TODO: Perhaps add xattr? */

	fetch_restore_meta_path(metapath, this_inode);
	fptr = fopen(metapath, "w+");
	if (!fptr) {
		write_log(0, "Error: Fail to create symlink in %s. Code %d",
				__func__, errno);
		return -errno;
	}
	setbuf(fptr, NULL);
	PWRITE(fileno(fptr), &symlink_header, sizeof(SYMLINK_META_HEADER), 0);
	fstat(fileno(fptr), &tmpstat);
	fclose(fptr);

	metasize = tmpstat.st_size;
	metasize_blk = tmpstat.st_blocks * 512;
	UPDATE_RECT_SYSMETA(.delta_system_size = -metasize,
			    .delta_meta_size = -metasize_blk,
			    .delta_pinned_size = 0,
			    .delta_backend_size = 0,
			    .delta_backend_meta_size = 0,
			    .delta_backend_inodes = 0);

	/* Mark dirty */
	FWRITE(&this_inode, sizeof(ino_t), 1, to_sync_fptr);
	write_log(4, "Successfully create symlink %s", pkgname);

	return 0;

errcode_handle:
	return errcode;
}
