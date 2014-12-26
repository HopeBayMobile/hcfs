#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "global.h"
#include "params.h"
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

int search_dir_entry_btree(char *target_name, DIR_ENTRY_PAGE *current_node, int fptr, int *result_index, DIR_ENTRY_PAGE *result_node)
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
  pread(fptr, &temp_page,sizeof(DIR_ENTRY_PAGE), current_node->child_page_pos[selected_index]);

  return search_dir_entry_btree(target_name, &temp_page, fptr, result_index, result_node);
 }

/* 
 Return val of insertion:
 0: Complete, no splitting
 1: Complete, contains overflow item
 -1: Not completed as entry already exists
*/
/* if returns 1, then there is an entry to be added to the parent */
int insert_dir_entry_btree(DIR_ENTRY *new_entry, DIR_ENTRY_PAGE *current_node, int fptr, DIR_ENTRY *overflow_median, long long *overflow_new_page, DIR_META_TYPE *this_meta, DIR_ENTRY *temp_dir_entries, long long *temp_child_page_pos)
 {
  int selected_index, ret_val, median_entry;
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
      pwrite(fptr, current_node, sizeof(DIR_ENTRY_PAGE), current_node->this_page_pos);
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

    /* Copy items to the left of median to the old node*/
    current_node->num_entries = median_entry;
    memcpy(current_node->dir_entries, temp_dir_entries, sizeof(DIR_ENTRY)*median_entry);

    /* Create a new node and copy all items to the right of median to the new node */
    if (this_meta->entry_page_gc_list != 0)
     {
      /*Reclaim node from gc list first*/
      pread(fptr,&newpage,sizeof(DIR_ENTRY_PAGE),this_meta->entry_page_gc_list);
      newpage.this_page_pos = this_meta->entry_page_gc_list;
      this_meta->entry_page_gc_list = newpage.gc_list_next;
     }
    else
     {
      memset(&newpage,0,sizeof(DIR_ENTRY_PAGE));
      newpage.this_page_pos = lseek(fptr,0,SEEK_END);
     }
    newpage.gc_list_next = 0;
    newpage.tree_walk_next = this_meta->tree_walk_list_head;
    newpage.tree_walk_prev = 0;

    if (this_meta->tree_walk_list_head == current_node->this_page_pos)
     {
      current_node->tree_walk_prev = newpage.this_page_pos;
     }
    else
     {
      pread(fptr,&temp_page2, sizeof(DIR_ENTRY_PAGE), this_meta->tree_walk_list_head);
      temp_page2.tree_walk_prev = newpage.this_page_pos;
      pwrite(fptr, &temp_page2, sizeof(DIR_ENTRY_PAGE), this_meta->tree_walk_list_head);
     }

    /*Write current node to disk*/
    pwrite(fptr, current_node, sizeof(DIR_ENTRY_PAGE),current_node->this_page_pos);

    this_meta->tree_walk_list_head = newpage.this_page_pos;
    pwrite(fptr, this_meta,sizeof(DIR_META_TYPE),sizeof(struct stat));

    /* Parent of new node is the same as the parent of the old node*/
    newpage.parent_page_pos = current_node->parent_page_pos;
    memset(newpage.child_page_pos,0,sizeof(long long)*(MAX_DIR_ENTRIES_PER_PAGE+1));
    newpage.num_entries = temp_total - median_entry - 1;
    memcpy(newpage.dir_entries,&(temp_dir_entries[median_entry+1]), sizeof(DIR_ENTRY)*newpage.num_entries);
    /* Write to disk after finishing */
    pwrite(fptr, &newpage,sizeof(DIR_ENTRY_PAGE),newpage.this_page_pos);

    /* Pass the median and the file pos of the new node to the parent*/
    *overflow_new_page = newpage.this_page_pos;
    printf("overflow %s\n",overflow_median->d_name);

    return 1;
   }
  else
   {
    /* Internal node. Prepare to go deeper */
    pread(fptr, &temppage, sizeof(DIR_ENTRY_PAGE), current_node->child_page_pos[selected_index]);
    ret_val = insert_dir_entry_btree(new_entry, &temppage, fptr, &tmp_overflow_median, &tmp_overflow_new_page, this_meta, temp_dir_entries, temp_child_page_pos);
    if (ret_val < 1)
     {
      /*Finished. Just return*/
      return ret_val;
     }
    printf("overflow up %s\n",tmp_overflow_median.d_name);

    /* Reload current node */
    pread(fptr, current_node, sizeof(DIR_ENTRY_PAGE), current_node->this_page_pos);

    /* If function return contains a median, insert to the current node */
    if (current_node->num_entries < MAX_DIR_ENTRIES_PER_PAGE)
     {
      printf("overflow up path a %s\n",tmp_overflow_median.d_name);
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
      pwrite(fptr,current_node, sizeof(DIR_ENTRY_PAGE), current_node->this_page_pos);
      return 0; /*Insertion completed*/
     }
    printf("overflow up path b %s\n",tmp_overflow_median.d_name);

    /*Need to split*/
    if (selected_index > 0)
     memcpy(temp_dir_entries, current_node->dir_entries,sizeof(DIR_ENTRY)*selected_index);
    memcpy(&(temp_dir_entries[selected_index]),&tmp_overflow_median,sizeof(DIR_ENTRY));
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

    /* Copy items to the left of median to the old node */
    current_node->num_entries = median_entry;
    memcpy(current_node->dir_entries, temp_dir_entries, sizeof(DIR_ENTRY)*median_entry);
    memcpy(current_node->child_page_pos, temp_child_page_pos, sizeof(long long)*(median_entry+1));

    /* Create a new node and copy all items to the right of median to the new node */
    if (this_meta->entry_page_gc_list != 0)
     {
      /*Reclaim node from gc list first*/
      pread(fptr, &newpage,sizeof(DIR_ENTRY_PAGE),this_meta->entry_page_gc_list);
      newpage.this_page_pos = this_meta->entry_page_gc_list;
      this_meta->entry_page_gc_list = newpage.gc_list_next;
     }
    else
     {
      memset(&newpage,0,sizeof(DIR_ENTRY_PAGE));
      newpage.this_page_pos = lseek(fptr,0,SEEK_END);
     }
    newpage.gc_list_next = 0;
    newpage.tree_walk_next = this_meta->tree_walk_list_head;
    newpage.tree_walk_prev = 0;

    if (this_meta->tree_walk_list_head == current_node->this_page_pos)
     {
      current_node->tree_walk_prev = newpage.this_page_pos;
     }
    else
     {
      pread(fptr, &temp_page2, sizeof(DIR_ENTRY_PAGE), this_meta->tree_walk_list_head);
      temp_page2.tree_walk_prev = newpage.this_page_pos;
      pwrite(fptr, &temp_page2, sizeof(DIR_ENTRY_PAGE), this_meta->tree_walk_list_head);
     }

    /*Write current node to disk*/
    pwrite(fptr, current_node, sizeof(DIR_ENTRY_PAGE),current_node->this_page_pos);

    this_meta->tree_walk_list_head = newpage.this_page_pos;
    pwrite(fptr, this_meta,sizeof(DIR_META_TYPE),sizeof(struct stat));

    /* Parent of new node is the same as the parent of the old node*/
    newpage.parent_page_pos = current_node->parent_page_pos;
    memset(newpage.child_page_pos,0,sizeof(long long)*(MAX_DIR_ENTRIES_PER_PAGE+1));
    newpage.num_entries = temp_total - median_entry - 1;
    memcpy(newpage.dir_entries,&(temp_dir_entries[median_entry+1]), sizeof(DIR_ENTRY)*newpage.num_entries);
    memcpy(newpage.child_page_pos, &(temp_child_page_pos[median_entry+1]), sizeof(long long)*(newpage.num_entries+1));

    /* Write to disk after finishing */
    pwrite(fptr, &newpage,sizeof(DIR_ENTRY_PAGE),newpage.this_page_pos);

    /* Pass the median and the file pos of the new node to the parent*/
    *overflow_new_page = newpage.this_page_pos;
    printf("overflow %s\n",overflow_median->d_name);

    return 1;
   }

  return 0;
 }

int delete_dir_entry_btree(DIR_ENTRY *to_delete_entry, DIR_ENTRY_PAGE *current_node, int fptr, DIR_META_TYPE *this_meta, DIR_ENTRY *temp_dir_entries, long long *temp_child_page_pos)
 {
  int selected_index, ret_val, entry_to_delete;
  DIR_ENTRY_PAGE temppage;
  DIR_ENTRY extracted_child;
  int temp_total;

  /*First search for the index to insert or traverse*/
  entry_to_delete = dentry_binary_search(current_node->dir_entries,current_node->num_entries, to_delete_entry, &selected_index);

  if (entry_to_delete >=0)
   {
    /* We found the element. Delete it. */

    if (current_node->child_page_pos[entry_to_delete] == 0)
     {
      /*We are now at the leaf node*/
      /* Just delete and return. Won't need to handle underflow here */
      memcpy(&(temp_dir_entries[0]), &(current_node->dir_entries[entry_to_delete+1]), sizeof(DIR_ENTRY)*((current_node->num_entries - entry_to_delete)-1));
      memcpy(&(current_node->dir_entries[entry_to_delete]), &(temp_dir_entries[0]), sizeof(DIR_ENTRY)*((current_node->num_entries - entry_to_delete)-1));
      current_node->num_entries--;

      pwrite(fptr, current_node, sizeof(DIR_ENTRY_PAGE), current_node->this_page_pos);

      return 0;
     }
    else
     {
      /*Select and remove the largest element from the left subtree of entry_to_delete*/
      /* Conduct rebalancing all the way down */

      /* First make sure the selected child is balanced */

      ret_val = rebalance_btree(current_node, fptr, this_meta, entry_to_delete, temp_dir_entries, temp_child_page_pos);

  /* If rebalanced, recheck by calling this function with the same parameters, else read the child node and go down the tree */
      if (ret_val > 0)
       {
        if (ret_val == 2) /* Need to reload current_node. Old one is deleted */
         {
          pread(fptr, &temppage, sizeof(DIR_ENTRY_PAGE),this_meta->root_entry_page);
          return delete_dir_entry_btree(to_delete_entry, &temppage, fptr, this_meta, temp_dir_entries, temp_child_page_pos);
         }
        else
         return delete_dir_entry_btree(to_delete_entry, current_node, fptr, this_meta, temp_dir_entries, temp_child_page_pos);
       }

      if (ret_val < 0)
       {
        printf ("debug dtree error in/after rebalancing\n");
        return ret_val;
       }
      
      pread(fptr, &temppage, sizeof(DIR_ENTRY_PAGE),current_node->child_page_pos[entry_to_delete]);

      ret_val = extract_largest_child(&temppage,fptr,this_meta, &extracted_child, temp_dir_entries, temp_child_page_pos);
      /* Replace the entry_to_delete with the largest element from the left subtree */
      if (ret_val < 0)
       {
        printf("debug error in finding largest child\n");
        return ret_val;
       }

      memcpy(&(current_node->dir_entries[entry_to_delete]), &extracted_child, sizeof(DIR_ENTRY));

      pwrite(fptr, current_node, sizeof(DIR_ENTRY_PAGE), current_node->this_page_pos);
      
      return 0;
     }
   }

  if (current_node->child_page_pos[selected_index] == 0)
   {
    printf("debug dtree cannot find the item\n");
    return -1;  /*Cannot find the item to delete. Return. */
   }

  /* Rebalance the selected child with its right sibling (or left if the rightmost) if needed */
  ret_val = rebalance_btree(current_node, fptr, this_meta, selected_index, temp_dir_entries, temp_child_page_pos);

  /* If rebalanced, recheck by calling this function with the same parameters, else read the child node and go down the tree */
  if (ret_val > 0)
   {
    if (ret_val == 2) /* Need to reload current_node. Old one is deleted */
     {
      pread(fptr, &temppage, sizeof(DIR_ENTRY_PAGE),this_meta->root_entry_page);
      return delete_dir_entry_btree(to_delete_entry, &temppage, fptr, this_meta, temp_dir_entries, temp_child_page_pos);
     }
    else
     return delete_dir_entry_btree(to_delete_entry, current_node, fptr, this_meta, temp_dir_entries, temp_child_page_pos);
   }

  if (ret_val < 0)
   {
    printf ("debug dtree error in/after rebalancing\n");
    return ret_val;
   }
       
  pread(fptr, &temppage, sizeof(DIR_ENTRY_PAGE),current_node->child_page_pos[selected_index]);
  
  return delete_dir_entry_btree(to_delete_entry, &temppage, fptr, this_meta, temp_dir_entries, temp_child_page_pos);
 }

int rebalance_btree(DIR_ENTRY_PAGE *current_node, int fptr, DIR_META_TYPE *this_meta, int selected_child, DIR_ENTRY *temp_dir_entries, long long *temp_child_page_pos)
 {
  /* How to rebalance: if num_entries of child <= MIN_DIR_ENTRIES_PER_PAGE, check if its right (or left) sibling contains child < MAX_DIR_ENTRIES_PER_PAGE / 2. If so, just merge the two children and the parent item in between (parent node lost one element). If the current node is the root and has only one element, make the merged node the new root and put the old root to the gc list.
If merging occurs, the dropped page goes to the gc list. Tree walk pointers are also updated
in this case.
   If the sibling contains child > MAX_DIR_ENTRIES_PER_PAGE / 2, pool the elements from
the two nodes, plus the parent item in between, and split the pooled elements into two,
using the median as the new parent item. */

  /* Returns 1 if rebalancing is conducted, and no new root is created. 
             2 if rebalancing is conducted, and there is a new root. The current_node of the caller should be reloaded in this case.
             0 if no rebalancing is needed.
             -1 if an error occurred. */
  int selected_sibling, ret_val, left_node, right_node, to_return;
  DIR_ENTRY_PAGE left_page, right_page, temp_page;
  DIR_ENTRY extracted_child;
  int temp_total, median_entry;
  char merging;

  if (current_node->child_page_pos[selected_child] <= 0)
   return -1;

  if (selected_child == current_node->num_entries)
   {
    /* If selected child is the rightmost one, sibling is the one to the left */
    pread(fptr, &right_page, sizeof(DIR_ENTRY_PAGE),current_node->child_page_pos[selected_child]);

    if (right_page.num_entries > MIN_DIR_ENTRIES_PER_PAGE)
     return 0;    /* No rebalancing needed */

    selected_sibling = selected_child - 1;
    left_node = selected_sibling;
    right_node = selected_child;
    pread(fptr, &left_page, sizeof(DIR_ENTRY_PAGE), current_node->child_page_pos[selected_sibling]);
    if (left_page.num_entries < MAX_DIR_ENTRIES_PER_PAGE / 2)
     merging = TRUE;
    else
     merging = FALSE;
   }
  else
   {
    pread(fptr, &left_page, sizeof(DIR_ENTRY_PAGE), current_node->child_page_pos[selected_child]);

    if (left_page.num_entries > MIN_DIR_ENTRIES_PER_PAGE)
     return 0;    /* No rebalancing needed */

    selected_sibling = selected_child + 1;
    left_node = selected_child;
    right_node = selected_sibling;
    pread(fptr, &right_page, sizeof(DIR_ENTRY_PAGE), current_node->child_page_pos[selected_sibling]);
    if (right_page.num_entries < MAX_DIR_ENTRIES_PER_PAGE / 2)
     merging = TRUE;
    else
     merging = FALSE;
   }

    /*First pool the items together */
  temp_total = left_page.num_entries + right_page.num_entries + 1;
  memcpy(&(temp_dir_entries[0]), &(left_page.dir_entries[0]), sizeof(DIR_ENTRY) * left_page.num_entries);
  memcpy(&(temp_child_page_pos[0]), &(left_page.child_page_pos[0]), sizeof(long long) * (left_page.num_entries+1));
  memcpy(&(temp_dir_entries[left_page.num_entries]),&(current_node->dir_entries[left_node]), sizeof(DIR_ENTRY));
  memcpy(&(temp_dir_entries[left_page.num_entries+1]), &(right_page.dir_entries[0]), sizeof(DIR_ENTRY) * right_page.num_entries);
  memcpy(&(temp_child_page_pos[left_page.num_entries+1]), &(right_page.child_page_pos[0]), sizeof(long long) * (right_page.num_entries+1));

  if (merging == TRUE)
   {
    /* Merge the two nodes and process node deletion */

    /* Copy the pooled items to the left node */
    memcpy(&(left_page.dir_entries[0]), &(temp_dir_entries[0]), sizeof(DIR_ENTRY) * temp_total);
    memcpy(&(left_page.child_page_pos[0]), &(temp_child_page_pos[0]), sizeof(long long) * (temp_total + 1));
    left_page.num_entries = temp_total;

    /* Drop the right node and update related info, including gc_list and tree walk pointer*/
    memset(&temp_page, 0, sizeof(DIR_ENTRY_PAGE));
    temp_page.this_page_pos = right_page.this_page_pos;
    temp_page.gc_list_next = this_meta->entry_page_gc_list;
    this_meta->entry_page_gc_list = temp_page.this_page_pos;
    pwrite(fptr, &temp_page, sizeof(DIR_ENTRY_PAGE), temp_page.this_page_pos);

    if (this_meta->tree_walk_list_head == right_page.this_page_pos)
     this_meta->tree_walk_list_head = right_page.tree_walk_next;

    if (right_page.tree_walk_next!=0)
     {
      if (right_page.tree_walk_next == left_page.this_page_pos)
       {
        left_page.tree_walk_prev = right_page.tree_walk_prev;
       }
      else
       {
        if (right_page.tree_walk_next == current_node->this_page_pos)
         {
          current_node->tree_walk_prev = right_page.tree_walk_prev;
         }
        else
         {
          pread(fptr, &temp_page, sizeof(DIR_ENTRY_PAGE), right_page.tree_walk_next);
          temp_page.tree_walk_prev = right_page.tree_walk_prev;
          pwrite(fptr, &temp_page, sizeof(DIR_ENTRY_PAGE), right_page.tree_walk_next);
         }
       }
     }
    if (right_page.tree_walk_prev!=0)
     {
      if (right_page.tree_walk_prev == left_page.this_page_pos)
       {
        left_page.tree_walk_next = right_page.tree_walk_next;
       }
      else
       {
        if (right_page.tree_walk_prev == current_node->this_page_pos)
         {
          current_node->tree_walk_next = right_page.tree_walk_next;
         }
        else
         {
          pread(fptr, &temp_page, sizeof(DIR_ENTRY_PAGE), right_page.tree_walk_prev);
          temp_page.tree_walk_next = right_page.tree_walk_next;
          pwrite(fptr, &temp_page, sizeof(DIR_ENTRY_PAGE), right_page.tree_walk_prev);
         }
       }
     }
    /*Decide whether we need to drop the root node and return 2, otherwise, update
      parent node*/
    if (current_node->num_entries == 1) /* We are dropping the only element in the parent*/
     {
      /* Drop root and make left_node the new root */
      memset(&temp_page, 0, sizeof(DIR_ENTRY_PAGE));
      temp_page.this_page_pos = current_node->this_page_pos;
      temp_page.gc_list_next = this_meta->entry_page_gc_list;
      this_meta->entry_page_gc_list = temp_page.this_page_pos;
      pwrite(fptr, &temp_page, sizeof(DIR_ENTRY_PAGE), temp_page.this_page_pos);

      if (this_meta->tree_walk_list_head == current_node->this_page_pos)
       this_meta->tree_walk_list_head = current_node->tree_walk_next;

      if (current_node->tree_walk_next == left_page.this_page_pos)
       {
        left_page.tree_walk_prev = current_node->tree_walk_prev;
       }
      else
       {
        if (current_node->tree_walk_next!=0)
         {
          pread(fptr, &temp_page, sizeof(DIR_ENTRY_PAGE), current_node->tree_walk_next);
          temp_page.tree_walk_prev = current_node->tree_walk_prev;
          pwrite(fptr, &temp_page, sizeof(DIR_ENTRY_PAGE), current_node->tree_walk_next);
         }
       }
      if (current_node->tree_walk_prev == left_page.this_page_pos)
       {
        left_page.tree_walk_next = current_node->tree_walk_next;
       }
      else
       {
        if (current_node->tree_walk_prev!=0)
         {
          pread(fptr, &temp_page, sizeof(DIR_ENTRY_PAGE), current_node->tree_walk_prev);
          temp_page.tree_walk_next = current_node->tree_walk_next;
          pwrite(fptr, &temp_page, sizeof(DIR_ENTRY_PAGE), current_node->tree_walk_prev);
         }
       }
      this_meta->root_entry_page = left_page.this_page_pos;
      left_page.parent_page_pos = 0;
      to_return = 2;
     }
    else
     {
      to_return = 1;
      /* Just drop the item merged to the left node from current_node */
      memcpy(&(temp_dir_entries[0]), &(current_node->dir_entries[left_node+1]), sizeof(DIR_ENTRY)*(current_node->num_entries - (left_node+1)));
        memcpy(&(current_node->dir_entries[left_node]), &(temp_dir_entries[0]), sizeof(DIR_ENTRY)*(current_node->num_entries - (left_node+1)));

      memcpy(&(temp_child_page_pos[0]), &(current_node->child_page_pos[left_node+2]), sizeof(long long)*(current_node->num_entries - (left_node+1)));
      memcpy(&(current_node->child_page_pos[left_node+1]), &(temp_child_page_pos[0]), sizeof(long long)*(current_node->num_entries - (left_node+1)));
      current_node->num_entries--;

      pwrite(fptr, current_node, sizeof(DIR_ENTRY_PAGE), current_node->this_page_pos);
     }

    /* Write changes to left node and meta to disk and return */
    pwrite(fptr, this_meta, sizeof(DIR_META_TYPE), sizeof(struct stat));

    pwrite(fptr, &left_page, sizeof(DIR_ENTRY_PAGE), left_page.this_page_pos);

    return to_return;
   }
  else
   {
    /* Split the pooled items into two, and replace the old parent in the middle with median */
    median_entry = temp_total / 2;

    /* Copy items to the left of the median to the left page and write to disk */
    memcpy(&(left_page.dir_entries[0]), &(temp_dir_entries[0]), sizeof(DIR_ENTRY) * median_entry);
    memcpy(&(left_page.child_page_pos[0]), &(temp_child_page_pos[0]), sizeof(long long) * (median_entry + 1));
    left_page.num_entries = median_entry;
    pwrite(fptr, &left_page, sizeof(DIR_ENTRY_PAGE), left_page.this_page_pos);

    /* Copy items to the right of the median to the right page and write to disk */
    memcpy(&(right_page.dir_entries[0]), &(temp_dir_entries[median_entry+1]), sizeof(DIR_ENTRY) * ((temp_total - median_entry)-1));
    memcpy(&(right_page.child_page_pos[0]), &(temp_child_page_pos[median_entry+1]), sizeof(long long) * (temp_total - median_entry));
    right_page.num_entries = (temp_total - median_entry)-1;
    pwrite(fptr, &right_page, sizeof(DIR_ENTRY_PAGE), right_page.this_page_pos);

    /* Write median to the current node and write to disk */
    memcpy(&(current_node->dir_entries[left_node]), &(temp_dir_entries[median_entry]), sizeof(DIR_ENTRY));
    pwrite(fptr, current_node, sizeof(DIR_ENTRY_PAGE), current_node->this_page_pos);
    
    return 1;
   }
 
  return 0;
 }

int extract_largest_child(DIR_ENTRY_PAGE *current_node, int fptr, DIR_META_TYPE *this_meta, DIR_ENTRY *extracted_child, DIR_ENTRY *temp_dir_entries, long long *temp_child_page_pos)
 {
  /*Select and remove the largest element from the left subtree of entry_to_delete*/
  /* Conduct rebalancing all the way down, using rebalance_btree function */
  /* Return the largest element using extracted_child pointer */

  int selected_index, ret_val, entry_to_delete;
  DIR_ENTRY_PAGE temppage;
  int temp_total;

  selected_index = current_node->num_entries;
  if (current_node->child_page_pos[selected_index] == 0)
   {
    /*We are now at the leaf node*/
    /* Just delete and return. Won't need to handle underflow here */
    memcpy(extracted_child, &(current_node->dir_entries[selected_index-1]), sizeof(DIR_ENTRY));
    current_node->num_entries--;

    pwrite(fptr, current_node, sizeof(DIR_ENTRY_PAGE), current_node->this_page_pos);

    return 0;
   }

  /* Conduct rebalancing all the way down */

  ret_val = rebalance_btree(current_node, fptr, this_meta, selected_index, temp_dir_entries, temp_child_page_pos);

  /* If rebalanced, recheck by calling this function with the same parameters, else read the child node and go down the tree */
  if (ret_val > 0)
   {
    if (ret_val == 2) /* Need to reload current_node. Old one is deleted */
     {
      pread(fptr, &temppage, sizeof(DIR_ENTRY_PAGE),this_meta->root_entry_page);
      return extract_largest_child(&temppage, fptr, this_meta, extracted_child, temp_dir_entries, temp_child_page_pos);
     }
    else
     return extract_largest_child(current_node, fptr, this_meta, extracted_child, temp_dir_entries, temp_child_page_pos);
   }

  if (ret_val < 0)
   return ret_val;
       
  pread(fptr, &temppage, sizeof(DIR_ENTRY_PAGE),current_node->child_page_pos[selected_index]);

  return extract_largest_child(&temppage,fptr,this_meta, extracted_child, temp_dir_entries, temp_child_page_pos);
 }

