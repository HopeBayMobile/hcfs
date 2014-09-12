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

    compare_result = strcmp(new_entry->d_name, entry_array[compare_entry]->d_name);
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

int search_dir_entry_btree(char *target_name, DIR_ENTRY_PAGE *current_node, FILE *fptr, DIR_ENTRY *result_entry, DIR_ENTRY_PAGE *result_node)
 {
  DIR_ENTRY temp_entry;
  int selected_index, ret_val;
  DIR_ENTRY_PAGE temp_page;

  strcpy(&(temp_entry.d_name), target_name);

  ret_val = dentry_binary_search(current_node->dir_entries, current_node->num_entries, &temp_entry, &selected_index);

  if (ret_val >=0)
   {
    memcpy(result_entry,&(current_node->dir_entries[ret_val]),sizeof(DIR_ENTRY));
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

  return search_dir_entry_btree(target_name, &temp_page, fptr, result_entry, result_node);
 }

/* 
 Return val of insertion:
 0: Complete but not written to disk
 1: Results written to disk due to splitting
 -1: Not completed as entry already exists
*/
int insert_dir_entry_btree(DIR_ENTRY *new_entry, DIR_ENTRY_PAGE *current_node, FILE *fptr, DIR_ENTRY_PAGE *result_page)
 {
  int selected_index, ret_val, median_entry;
  DIR_ENTRY temp_dir_entries[MAX_DIR_ENTRIES_PER_PAGE+2];
  long long temp_child_page_pos[MAX_DIR_ENTRIES_PER_PAGE+3];

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
      memcpy(result_page, current_node, sizeof(DIR_ENTRY_PAGE));
      return 0; /*Insertion completed, and updated page is not written to disk*/
     }

    /*Need to split*/
    if (selected_index > 0)
      memcpy(temp_dir_entries, current_node->dir_entries,sizeof(DIR_ENTRY)*selected_index);
    memcpy(&(temp_dir_entries[selected_index]),new_entry,sizeof(DIR_ENTRY));
    if (selected_index < current_node->num_entries)
     memcpy(&(temp_dir_entries[selected_index+1]), &(current_node->dir_entries[selected_index]), sizeof(DIR_ENTRY)*(current_node->num_entries - selected_index));

    /* Select median */
    median_entry = (current_node->num_entries + 1) / 2;

    /* Copy items to the left of median to the old node, and write to disk */

    /* Create a new node and copy all items to the right of median to the new node */
    /* Parent of new node is the same as the parent of the old node*/
    /* Reclaim node from gc list first if any */
    /* Write to disk after finishing */

    /* Prepare to pass the median to the parent, but do not return here */

   }
  else
   {
    /* Internal node. Prepare to go deeper */

    /* If function return contains a median, insert to the current node */

    /* If overflow, prepare to split pass the median to the parent, but do not return here */

   }

  /* If parent is zero (i.e., this is root), create a new node as root and put in
     the median if splitting. Write to disk after finishing */

  /* Otherwise return the result*/

  return 1;
 }
