#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "fuseop.h"

/* TODO: How to integrate dir page reading / updating with mem cache? */

int dentry_binary_search(DIR_ENTRY *entry_array, int num_entries, DIR_ENTRY *new_entry, int *index_to_insert)
 {
  int compare_entry, compare_result;
  int start_index, end_index;

  if (num_entries <1)
   {
    *index_to_insert = 0;
    return -1;   /*Insert or traverse before the first element*/
   }

  start_index = 0;
  end_index = num_entries;
  while (end_index > start_index) /*If there is something left to compare to*/
   {
    if (end_index == (start_index + 1))
     compare_entry = start_index;
    else
     compare_entry = (end_index + start_index) / 2; /*Index of the element to compare to */

    if (compare_entry < 0)
     compare_entry = 0;
    if (compare_entry >=num_entries)
     compare_entry = num_entries - 1;

    compare_result = strcmp(new_entry->d_name, entry_array[compare_entry].d_name);
    if (compare_result == 0)
     {
      /* Entry is the same */
      return compare_entry;
     }

    if (compare_result < 0)
     {
      /*New element belongs to the left of the entry being compared to */
      end_index = compare_entry;
     }
    else
     {
      start_index = compare_entry + 1;
     }
   }
  *index_to_insert = start_index;
  return -1;  /*Not found. Returns where to insert the new entry*/
 }

int search_dir_entry_btree(char *target_name, DIR_ENTRY_PAGE *current_node, FILE *fptr, int *result_index, DIR_ENTRY_PAGE *result_node)
 {
  DIR_ENTRY temp_entry;
  int selected_index, ret_val;
  DIR_ENTRY_PAGE temp_page;

  strcpy(temp_entry.d_name, target_name);

  ret_val = dentry_binary_search(current_node->dir_entries, current_node->num_entries, &temp_entry, &selected_index);

  if (ret_val >=0)
   {
    *result_index = ret_val;
    memcpy(result_node, current_node, sizeof(DIR_ENTRY_PAGE));
    return ret_val;
   }
  /* Not in the current node. First check if we can dig deeper*/

  if (current_node->child_page_pos[selected_index] == 0)
   {
    /*Already at leaf*/
    return -1;   /*Item not found*/
   }

  /*Load child node*/
  fseek(fptr,current_node->child_page_pos[selected_index],SEEK_SET);
  fread(&temp_page,sizeof(DIR_ENTRY_PAGE),1,fptr);

  return search_dir_entry_btree(target_name, &temp_page, fptr, result_index, result_node);
 }

/* 
 Return val of insertion:
 0: Complete, no splitting
 1: Complete, contains overflow item
 -1: Not completed as entry already exists
*/
/* if returns 1, then there is an entry to be added to the parent */
int insert_dir_entry_btree(DIR_ENTRY *new_entry, DIR_ENTRY_PAGE *current_node, FILE *fptr, DIR_ENTRY *overflow_median, long long *overflow_new_page, DIR_META_TYPE *this_meta)
 {
  int selected_index, ret_val, median_entry;
  DIR_ENTRY temp_dir_entries[MAX_DIR_ENTRIES_PER_PAGE+2];
  long long temp_child_page_pos[MAX_DIR_ENTRIES_PER_PAGE+3];
  DIR_ENTRY_PAGE newpage,temppage, temp_page2;
  DIR_ENTRY tmp_overflow_median;
  int temp_total;
  long long tmp_overflow_new_page;

  /*First search for the index to insert or traverse*/
  ret_val = dentry_binary_search(current_node->dir_entries,current_node->num_entries, new_entry, &selected_index);

  if (ret_val >=0)
   return -1;   /*If entry already in the tree, return nothing */

  if (current_node->child_page_pos[selected_index] == 0)
   {
    /*We are now at the leaf node*/
    if (current_node->num_entries < MAX_DIR_ENTRIES_PER_PAGE)
     {
      /*Can add new entry to this node*/
      /*First shift the elements to the right of the point to insert*/
      if (selected_index < current_node->num_entries)
       {
        memcpy(&(temp_dir_entries[0]), &(current_node->dir_entries[selected_index]), sizeof(DIR_ENTRY)*(current_node->num_entries - selected_index));
        memcpy(&(current_node->dir_entries[selected_index+1]), &(temp_dir_entries[0]), sizeof(DIR_ENTRY)*(current_node->num_entries - selected_index));
       }
      /*Insert the new element*/
      memcpy(&(current_node->dir_entries[selected_index]),new_entry,sizeof(DIR_ENTRY));

      (current_node->num_entries)++;
      fseek(fptr, current_node->this_page_pos, SEEK_SET);
      fwrite(current_node, sizeof(DIR_ENTRY_PAGE), 1, fptr);
      return 0; /*Insertion completed*/
     }

    /*Need to split*/
    if (selected_index > 0)
     memcpy(temp_dir_entries, current_node->dir_entries,sizeof(DIR_ENTRY)*selected_index);
    memcpy(&(temp_dir_entries[selected_index]),new_entry,sizeof(DIR_ENTRY));
    if (selected_index < current_node->num_entries)
     memcpy(&(temp_dir_entries[selected_index+1]), &(current_node->dir_entries[selected_index]), sizeof(DIR_ENTRY)*(current_node->num_entries - selected_index));

    /* Select median */
    median_entry = (current_node->num_entries + 1) / 2;
    temp_total = current_node->num_entries + 1;

    /* Copy the median */
    memcpy(overflow_median, &(temp_dir_entries[median_entry]),sizeof(DIR_ENTRY));

    /* Copy items to the left of median to the old node, and write to disk */
    current_node->num_entries = median_entry;
    memcpy(current_node->dir_entries, temp_dir_entries, sizeof(DIR_ENTRY)*median_entry);
    fseek(fptr,current_node->this_page_pos,SEEK_SET);
    fwrite(current_node, sizeof(DIR_ENTRY_PAGE),1,fptr);

    /* Create a new node and copy all items to the right of median to the new node */
    if (this_meta->entry_page_gc_list != 0)
     {
      /*Reclaim node from gc list first*/
      fseek(fptr, this_meta->entry_page_gc_list, SEEK_SET);
      fread(&newpage,sizeof(DIR_ENTRY_PAGE),1,fptr);
      newpage.this_page_pos = this_meta->entry_page_gc_list;
      this_meta->entry_page_gc_list = newpage.gc_list_next;
     }
    else
     {
      memset(&newpage,0,sizeof(DIR_ENTRY_PAGE));
      fseek(fptr,0,SEEK_END);
      newpage.this_page_pos = ftell(fptr);
     }
    newpage.gc_list_next = 0;
    newpage.tree_walk_next = this_meta->tree_walk_list_head;
    newpage.tree_walk_prev = 0;
    fseek(fptr, this_meta->tree_walk_list_head, SEEK_SET);
    fread(&temp_page2, sizeof(DIR_ENTRY_PAGE), 1, fptr);
    temp_page2.tree_walk_prev = newpage.this_page_pos;
    fseek(fptr, this_meta->tree_walk_list_head, SEEK_SET);
    fwrite(&temp_page2, sizeof(DIR_ENTRY_PAGE), 1, fptr);

    this_meta->tree_walk_list_head = newpage.this_page_pos;
    fseek(fptr,sizeof(struct stat), SEEK_SET);
    fwrite(this_meta,sizeof(DIR_META_TYPE),1,fptr);

    /* Parent of new node is the same as the parent of the old node*/
    newpage.parent_page_pos = current_node->parent_page_pos;
    memset(newpage.child_page_pos,0,sizeof(long long)*(MAX_DIR_ENTRIES_PER_PAGE+1));
    newpage.num_entries = temp_total - median_entry - 1;
    memcpy(newpage.dir_entries,&(temp_dir_entries[median_entry+1]), sizeof(DIR_ENTRY)*newpage.num_entries);
    /* Write to disk after finishing */
    fseek(fptr, newpage.this_page_pos, SEEK_SET);
    fwrite(&newpage,sizeof(DIR_ENTRY_PAGE),1,fptr);

    /* Pass the median and the file pos of the new node to the parent*/
    *overflow_new_page = newpage.this_page_pos;
    printf("overflow %s\n",overflow_median->d_name);

    return 1;
   }
  else
   {
    /* Internal node. Prepare to go deeper */
    fseek(fptr, current_node->child_page_pos[selected_index], SEEK_SET);
    fread(&temppage, sizeof(DIR_ENTRY_PAGE), 1, fptr);
    ret_val = insert_dir_entry_btree(new_entry, &temppage, fptr, &tmp_overflow_median, &tmp_overflow_new_page, this_meta);
    if (ret_val < 1)
     {
      /*Finished. Just return*/
      return ret_val;
     }
    printf("overflow up %s\n",tmp_overflow_median.d_name);

    /* If function return contains a median, insert to the current node */
    if (current_node->num_entries < MAX_DIR_ENTRIES_PER_PAGE)
     {
      /*Can add new entry to this node*/
      /*First shift the elements to the right of the point to insert*/
      if (selected_index < current_node->num_entries)
       {
        memcpy(&(temp_dir_entries[0]), &(current_node->dir_entries[selected_index]), sizeof(DIR_ENTRY)*(current_node->num_entries - selected_index));
        memcpy(&(current_node->dir_entries[selected_index+1]), &(temp_dir_entries[0]), sizeof(DIR_ENTRY)*(current_node->num_entries - selected_index));

        memcpy(&(temp_child_page_pos[0]), &(current_node->child_page_pos[selected_index+1]), sizeof(long long)*(current_node->num_entries - selected_index));
        memcpy(&(current_node->child_page_pos[selected_index+2]), &(temp_child_page_pos[0]), sizeof(long long)*(current_node->num_entries - selected_index));

       }
      /*Insert the overflow element*/
      memcpy(&(current_node->dir_entries[selected_index]),&tmp_overflow_median,sizeof(DIR_ENTRY));
      current_node->child_page_pos[selected_index+1] = tmp_overflow_new_page;

      (current_node->num_entries)++;
      fseek(fptr, current_node->this_page_pos, SEEK_SET);
      fwrite(current_node, sizeof(DIR_ENTRY_PAGE), 1, fptr);
      return 0; /*Insertion completed*/
     }

    /*Need to split*/
    if (selected_index > 0)
     memcpy(temp_dir_entries, current_node->dir_entries,sizeof(DIR_ENTRY)*selected_index);
    memcpy(&(temp_dir_entries[selected_index]),new_entry,sizeof(DIR_ENTRY));
    if (selected_index < current_node->num_entries)
     memcpy(&(temp_dir_entries[selected_index+1]), &(current_node->dir_entries[selected_index]), sizeof(DIR_ENTRY)*(current_node->num_entries - selected_index));

    if (selected_index > 0)
     memcpy(temp_child_page_pos, current_node->child_page_pos,sizeof(long long)*(selected_index+1));
    temp_child_page_pos[selected_index+1] = tmp_overflow_new_page;
    if (selected_index < current_node->num_entries)
     memcpy(&(temp_child_page_pos[selected_index+2]), &(current_node->child_page_pos[selected_index+1]), sizeof(long long)*(current_node->num_entries - selected_index));

    /* Select median */
    median_entry = (current_node->num_entries + 1) / 2;
    temp_total = current_node->num_entries + 1;

    /* Copy the median */
    memcpy(overflow_median, &(temp_dir_entries[median_entry]),sizeof(DIR_ENTRY));

    /* Copy items to the left of median to the old node, and write to disk */
    current_node->num_entries = median_entry;
    memcpy(current_node->dir_entries, temp_dir_entries, sizeof(DIR_ENTRY)*median_entry);
    memcpy(current_node->child_page_pos, temp_child_page_pos, sizeof(long long)*(median_entry+1));
    fseek(fptr,current_node->this_page_pos,SEEK_SET);
    fwrite(current_node, sizeof(DIR_ENTRY_PAGE),1,fptr);

    /* Create a new node and copy all items to the right of median to the new node */
    if (this_meta->entry_page_gc_list != 0)
     {
      /*Reclaim node from gc list first*/
      fseek(fptr, this_meta->entry_page_gc_list, SEEK_SET);
      fread(&newpage,sizeof(DIR_ENTRY_PAGE),1,fptr);
      newpage.this_page_pos = this_meta->entry_page_gc_list;
      this_meta->entry_page_gc_list = newpage.gc_list_next;
     }
    else
     {
      memset(&newpage,0,sizeof(DIR_ENTRY_PAGE));
      fseek(fptr,0,SEEK_END);
      newpage.this_page_pos = ftell(fptr);
     }
    newpage.gc_list_next = 0;
    newpage.tree_walk_next = this_meta->tree_walk_list_head;
    newpage.tree_walk_prev = 0;
    fseek(fptr, this_meta->tree_walk_list_head, SEEK_SET);
    fread(&temp_page2, sizeof(DIR_ENTRY_PAGE), 1, fptr);
    temp_page2.tree_walk_prev = newpage.this_page_pos;
    fseek(fptr, this_meta->tree_walk_list_head, SEEK_SET);
    fwrite(&temp_page2, sizeof(DIR_ENTRY_PAGE), 1, fptr);

    this_meta->tree_walk_list_head = newpage.this_page_pos;
    fseek(fptr,sizeof(struct stat), SEEK_SET);
    fwrite(this_meta,sizeof(DIR_META_TYPE),1,fptr);

    /* Parent of new node is the same as the parent of the old node*/
    newpage.parent_page_pos = current_node->parent_page_pos;
    memset(newpage.child_page_pos,0,sizeof(long long)*(MAX_DIR_ENTRIES_PER_PAGE+1));
    newpage.num_entries = temp_total - median_entry - 1;
    memcpy(newpage.dir_entries,&(temp_dir_entries[median_entry+1]), sizeof(DIR_ENTRY)*newpage.num_entries);
    memcpy(newpage.child_page_pos, &(temp_child_page_pos[median_entry+1]), sizeof(long long)*(newpage.num_entries+1));

    /* Write to disk after finishing */
    fseek(fptr, newpage.this_page_pos, SEEK_SET);
    fwrite(&newpage,sizeof(DIR_ENTRY_PAGE),1,fptr);

    /* Pass the median and the file pos of the new node to the parent*/
    *overflow_new_page = newpage.this_page_pos;

    return 1;
   }

  return 0;
 }
