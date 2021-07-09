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
#ifndef GW20_HCFS_DIR_ENTRY_BTREE_H_
#define GW20_HCFS_DIR_ENTRY_BTREE_H_

#include "meta.h"
int32_t dentry_binary_search(const DIR_ENTRY *entry_array, const int32_t num_entries,
			const DIR_ENTRY *new_entry, int32_t *index_to_insert,
			BOOL is_external);

int32_t search_dir_entry_btree(const char *target_name, DIR_ENTRY_PAGE *tnode,
		int32_t fh, int32_t *result_index, DIR_ENTRY_PAGE *result_node,
		BOOL is_external);

/* if returns 1, then there is an entry to be added to the parent */
int32_t insert_dir_entry_btree(DIR_ENTRY *new_entry, DIR_ENTRY_PAGE *tnode,
	int32_t fh, DIR_ENTRY *overflow_median, int64_t *overflow_new_page,
	DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
	int64_t *temp_child_page_pos, BOOL is_external, int64_t meta_pos);

int32_t delete_dir_entry_btree(DIR_ENTRY *to_delete_entry, DIR_ENTRY_PAGE *tnode,
	int32_t fh, DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
	int64_t *temp_child_page_pos, BOOL is_external, int64_t meta_pos);

int32_t rebalance_btree(DIR_ENTRY_PAGE *tnode, int32_t fh, DIR_META_TYPE *this_meta,
	int32_t selected_child, DIR_ENTRY *tmp_entries,
	int64_t *temp_child_page_pos, int64_t meta_pos);

int32_t extract_largest_child(DIR_ENTRY_PAGE *tnode, int32_t fh,
	DIR_META_TYPE *this_meta, DIR_ENTRY *extracted_child,
	DIR_ENTRY *tmp_entries, int64_t *temp_child_page_pos,
	int64_t meta_pos);

#endif  /* GW20_HCFS_DIR_ENTRY_BTREE_H_ */
