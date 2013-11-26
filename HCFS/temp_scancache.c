#include <dirent.h>
#include "params.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    time_t access_time;
    int sub_dir_index;
  } SUBDIR_SORT_TYPE;

SUBDIR_SORT_TYPE dirsort_array[NUMSUBDIR];
int total_subdir;

int is_subdir(const struct dirent *)
 {
  if (!strncmp(dirent->d_name,"sub_",4))
   return 1;
  return 0;
 }
int is_block(const struct dirent *)
 {
  if (!strncmp(dirent->d_name,"block",5))
   return 1;
  return 0;
 }

static int comp_access_time(const void *sub1, const void *sub2)
 {
  SUBDIR_SORT_TYPE *ptr1,*ptr2;

  ptr1 = (SUBDIR_SORT_TYPE *) sub1;
  ptr2 = (SUBDIR_SORT_TYPE *) sub2;

  if (ptr1->access_time > ptr2->access_time)
   return 1;
  if (ptr1->access_time < ptr2->access_time)
   return -1;

  return 0;
 }

void create_subdir_sort()
 {
  char this_subdir_path[400];
  int count;

  total_subdir = 0;
  for(count = 0;
