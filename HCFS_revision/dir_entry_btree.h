
int dentry_binary_search(DIR_ENTRY *entry_array, int num_entries, DIR_ENTRY *new_entry, int *index_to_insert);

int search_dir_entry_btree(char *target_name, DIR_ENTRY_PAGE *current_node, FILE *fptr, int *result_index, DIR_ENTRY_PAGE *result_node);

/* if returns 1, then there is an entry to be added to the parent */
int insert_dir_entry_btree(DIR_ENTRY *new_entry, DIR_ENTRY_PAGE *current_node, FILE *fptr, DIR_ENTRY *overflow_median, long long *overflow_new_page, DIR_META_TYPE *this_meta);
