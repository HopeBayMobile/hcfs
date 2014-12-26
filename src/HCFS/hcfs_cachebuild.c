#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <time.h>
#include <curl/curl.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <attr/xattr.h>
#include <sys/mman.h>

#include "hcfs_cache.h"
#include "params.h"
#include "fuseop.h"
#include "super_block.h"

extern SYSTEM_CONF_STRUCT system_config;

void cache_usage_hash_init()
 {
  int count;
  CACHE_USAGE_NODE *node_ptr, *temp_ptr;

  nonempty_cache_hash_entries=0;
  for(count=0;count<CACHE_USAGE_NUM_ENTRIES;count++)
   {
    node_ptr = inode_cache_usage_hash[count];
    while(node_ptr!=NULL)
     {
      temp_ptr=node_ptr;
      node_ptr=node_ptr->next_node;
      free(temp_ptr);
     }
    inode_cache_usage_hash[count]=NULL;
   }
  return;
 }

CACHE_USAGE_NODE* return_cache_usage_node(ino_t this_inode)
 {
  int hash_value;
  CACHE_USAGE_NODE *node_ptr,*next_ptr;

  hash_value = this_inode % CACHE_USAGE_NUM_ENTRIES;

  node_ptr = inode_cache_usage_hash[hash_value];

  if (node_ptr == NULL)
   return NULL;

  if (node_ptr->this_inode == this_inode)
   {
    inode_cache_usage_hash[hash_value] = node_ptr->next_node;
    if (node_ptr->next_node == NULL)
     nonempty_cache_hash_entries--;
    node_ptr->next_node = NULL;
    return node_ptr;
   }

  next_ptr = node_ptr->next_node;

  while(next_ptr!=NULL)
   {
    if (next_ptr->this_inode == this_inode)
     {
      node_ptr->next_node = next_ptr->next_node;
      next_ptr->next_node = NULL;
      return next_ptr;
     }
    else
     {
      node_ptr = next_ptr;
      next_ptr = next_ptr->next_node;
     }
   }

  return NULL;
 }

void insert_cache_usage_node(ino_t this_inode, CACHE_USAGE_NODE *this_node)
 {
  int hash_value;
  CACHE_USAGE_NODE *node_ptr,*next_ptr;

  hash_value = this_inode % CACHE_USAGE_NUM_ENTRIES;

  node_ptr = inode_cache_usage_hash[hash_value];

  if (node_ptr == NULL) /*First one, so just insert*/
   {
    inode_cache_usage_hash[hash_value] = this_node;
    this_node->next_node = NULL;
    nonempty_cache_hash_entries++;
    return;
   }

  if (compare_cache_usage(this_node,node_ptr)<=0) /*The new node is the first one*/
   {
    this_node->next_node = node_ptr;
    inode_cache_usage_hash[hash_value] = this_node;
    return;
   }

  next_ptr = node_ptr->next_node;

  while(next_ptr!=NULL)
   {
    if (compare_cache_usage(this_node,next_ptr)>0) /*If insert new node at a latter place */
     {
      node_ptr = next_ptr;
      next_ptr = next_ptr->next_node;
     }
    else
     break;
   }

  node_ptr->next_node = this_node;
  this_node->next_node = next_ptr;
  return;
 }

int compare_cache_usage(CACHE_USAGE_NODE *first_node, CACHE_USAGE_NODE *second_node)
 {
  time_t first_node_time,second_node_time;

  /*If clean cache size is zero, put it to the end of the linked lists*/
  if (first_node->clean_cache_size ==0)
   return 1;

  if (second_node->clean_cache_size ==0)
   return -1;

  if (first_node->last_access_time > first_node->last_mod_time)
   first_node_time = first_node->last_access_time;
  else
   first_node_time = first_node->last_mod_time;

  if (second_node->last_access_time > second_node->last_mod_time)
   second_node_time = second_node->last_access_time;
  else
   second_node_time = second_node->last_mod_time;

  /*Use access/mod time to rank*/

  if (first_node_time > (second_node_time + 60))
   return 1;

  if (second_node_time > (first_node_time + 60))
   return -1;

  /*Use clean cache size to rank*/

  if (first_node->clean_cache_size > (second_node->clean_cache_size + MAX_BLOCK_SIZE))
   return -1;

  if (second_node->clean_cache_size > (first_node->clean_cache_size + MAX_BLOCK_SIZE))
   return 1;

  /*Use dirty cache size to rank*/

  if (first_node->dirty_cache_size > second_node->dirty_cache_size)
   return 1;

  if (first_node->dirty_cache_size < second_node->dirty_cache_size)
   return -1;

  return 0;
 }

void build_cache_usage()
 {
  char blockpath[400];
  char thisblockpath[400];
  DIR *dirptr;
  int count;
  struct dirent temp_dirent;
  struct dirent *direntptr;
  int ret_val;
  long long blockno;
  ino_t this_inode;
  struct stat tempstat;
  CACHE_USAGE_NODE *tempnode;
  char tempval[10];

  printf("Building cache usage hash table\n");
  cache_usage_hash_init();

  for(count=0;count<NUMSUBDIR;count++)
   {
    sprintf(blockpath,"%s/sub_%d",BLOCKPATH,count);

    if (access(blockpath,F_OK)<0)
     continue;

    dirptr=opendir(blockpath);

    readdir_r(dirptr,&temp_dirent,&direntptr);
    while(direntptr!=NULL)
     {
      ret_val = sscanf(temp_dirent.d_name,"block%lld_%lld",&this_inode, &blockno);
      if (ret_val == 2)
       {
        fetch_block_path(thisblockpath,this_inode,blockno);
        ret_val = stat(thisblockpath,&tempstat);

        if (ret_val == 0)
         {
          tempnode=return_cache_usage_node(this_inode);
          if (tempnode == NULL)
           {
            tempnode = malloc(sizeof(CACHE_USAGE_NODE));
            memset(tempnode,0,sizeof(CACHE_USAGE_NODE));
           }
          if (tempnode->last_access_time < tempstat.st_atime)
           tempnode->last_access_time = tempstat.st_atime;

          if (tempnode->last_mod_time < tempstat.st_mtime)
           tempnode->last_mod_time = tempstat.st_mtime;

          tempnode->this_inode = this_inode;

          getxattr(thisblockpath,"user.dirty",(void *)tempval,1);

          if (!strncmp(tempval,"T",1))  /*If this is dirty cache entry*/
           {
            tempnode->dirty_cache_size +=tempstat.st_size;
           }
          else
           {
            if (!strncmp(tempval,"F",1)) /*If clean cache entry*/
             {
              tempnode->clean_cache_size +=tempstat.st_size;
             }
            /*Otherwise, don't know the status of the block, so do nothing*/
           }
          insert_cache_usage_node(this_inode, tempnode);
         }
       }
      readdir_r(dirptr,&temp_dirent,&direntptr);
     }
    closedir(dirptr);
   }
  return;
 }
