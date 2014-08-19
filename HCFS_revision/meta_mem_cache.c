#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "params.h"
#include "fuseop.h"
#include "super_inode.h"
#include "meta_mem_cache.h"

META_CACHE_HEADER_STRUCT *meta_mem_cache;

int init_meta_cache_headers()
 {
  int count;
  int ret_val;

  meta_mem_cache = malloc(sizeof(META_CACHE_HEADER_STRUCT)*NUM_META_MEM_CACHE_HEADERS);

  if (meta_mem_cache == NULL)
   return -1;

  memset(meta_mem_cache,0,sizeof(META_CACHE_HEADER_STRUCT)*NUM_META_MEM_CACHE_HEADERS);

  for(count=0;count<NUM_META_MEM_CACHE_HEADERS;count++)
   {
    ret_val = sem_init(&(meta_mem_cache[count].header_sem),0,1);
    if (ret_val < 0)
     {
      free(meta_mem_cache);
      return -1;
     }
   }

  return 0;
 }

int release_meta_cache_headers()
 {
  int ret_val;

  if (meta_mem_cache == NULL)
   return -1;

  ret_val = flush_clean_all_meta_cache();  

  free(meta_mem_cache);
  return 0;
 }

int flush_single_meta_cache_entry(META_CACHE_LOOKUP_ENTRY_STRUCT *entry_ptr)
 {
  SUPER_INODE_ENTRY tempentry;
  int ret_val;
  char thismetapath[METAPATHLEN];
  FILE *fptr;
  int ret_code;
  META_CACHE_ENTRY_STRUCT *body_ptr;

  sem_wait(&((entry_ptr->cache_entry_body).access_sem));

  if (entry_ptr->something_dirty == FALSE)
   {
    sem_post(&((entry_ptr->cache_entry_body).access_sem));
    return 0;
   }

  body_ptr = &(entry_ptr->cache_entry_body);

  fetch_meta_path(thismetapath,entry_ptr->inode_num);

  fptr = fopen(thismetapath,"r+");
  if (fptr==NULL)
   {
    sem_post(&((entry_ptr->cache_entry_body).access_sem));
    return -ENOENT;
   }
  setbuf(fptr,NULL);
  
  flock(fileno(fptr),LOCK_EX);

  if (body_ptr->stat_dirty == TRUE)
   {
    fseek(fptr,0,SEEK_SET);
    fwrite(&(body_ptr->this_stat),sizeof(struct stat),1,fptr);
   }

  if (S_ISREG(body_ptr->inode_mode) == TRUE)
   {
    if (body_ptr->meta_dirty == TRUE)
     {
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fwrite((body_ptr->file_meta),sizeof(FILE_META_TYPE),1,fptr);
     }
    if ((body_ptr->block_entry_cache_dirty[0] == TRUE) && (body_ptr->block_entry_cache[0] != NULL))
     {
      fseek(fptr,body_ptr->block_entry_cache_pos[0],SEEK_SET);
      fwrite(body_ptr->block_entry_cache[0],sizeof(BLOCK_ENTRY_PAGE),1,fptr);
     }
    if ((body_ptr->block_entry_cache_dirty[1] == TRUE) && (body_ptr->block_entry_cache[1] != NULL))
     {
      fseek(fptr,body_ptr->block_entry_cache_pos[1],SEEK_SET);
      fwrite(body_ptr->block_entry_cache[1],sizeof(BLOCK_ENTRY_PAGE),1,fptr);
     }
   }

  if (S_ISDIR(body_ptr->inode_mode) == TRUE)
   {
    if (body_ptr->meta_dirty == TRUE)
     {
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fwrite((body_ptr->dir_meta),sizeof(DIR_META_TYPE),1,fptr);
     }
    if ((body_ptr->dir_entry_cache_dirty[0] == TRUE) && (body_ptr->dir_entry_cache[0] != NULL))
     {
      fseek(fptr,body_ptr->dir_entry_cache_pos[0],SEEK_SET);
      fwrite(body_ptr->dir_entry_cache[0],sizeof(DIR_ENTRY_PAGE),1,fptr);
     }
    if ((body_ptr->dir_entry_cache_dirty[1] == TRUE) && (body_ptr->dir_entry_cache[1] != NULL))
     {
      fseek(fptr,body_ptr->dir_entry_cache_pos[1],SEEK_SET);
      fwrite(body_ptr->dir_entry_cache[1],sizeof(DIR_ENTRY_PAGE),1,fptr);
     }
   }
   
  /*TODO: Add flush of xattr pages here */

  flock(fileno(fptr),LOCK_UN);
  fclose(fptr);
  if (body_ptr->stat_dirty == TRUE)
   super_inode_update_stat(entry_ptr->inode_num, &(body_ptr->this_stat));

  sem_post(&((entry_ptr->cache_entry_body).access_sem));

  return 0;
 }
int flush_clean_all_meta_cache() /* Flush all dirty entries and free memory usage */
 {
  int count;
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr,*old_ptr;

  for(count=0;count<NUM_META_MEM_CACHE_HEADERS;count++)
   {
    sem_wait(&(meta_mem_cache[count].header_sem));
    if (meta_mem_cache[count].num_entries > 0)
     {
      current_ptr = meta_mem_cache[count].meta_cache_entries;
      while(current_ptr!=NULL)
       {
        if (current_ptr->something_dirty == TRUE)
         {
          flush_single_meta_cache_entry(current_ptr);
         }
        free_single_meta_cache_entry(current_ptr);
        old_ptr = current_ptr;
        current_ptr = current_ptr->next;
        free(old_ptr);
       }
     }
    sem_post(&(meta_mem_cache[count].header_sem));
   }
  return 0;
 }

int free_single_meta_cache_entry(META_CACHE_LOOKUP_ENTRY_STRUCT *entry_ptr)
 {
  META_CACHE_ENTRY_STRUCT *entry_body;

  entry_body = &(entry_ptr->cache_entry_body);

  if (entry_body->dir_entry_cache[0] !=NULL)
   free(entry_body->dir_entry_cache[0]);
  if (entry_body->dir_entry_cache[1] !=NULL)
   free(entry_body->dir_entry_cache[1]);

  if (entry_body->block_entry_cache[0] !=NULL)
   free(entry_body->block_entry_cache[0]);
  if (entry_body->block_entry_cache[1] !=NULL)
   free(entry_body->block_entry_cache[1]);

  return 0;
 }

int hash_inode_to_meta_cache(ino_t this_inode)
 {
  return ((int) this_inode % NUM_META_MEM_CACHE_HEADERS);
 }

int meta_cache_lookup_stat(ino_t this_inode, struct stat *returned_stat)
 {
  int index;
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr;

  index = hash_inode_to_meta_cache(this_inode);
/*First lock corresponding header*/
  sem_wait(&(meta_mem_cache[index].header_sem));

  current_ptr = meta_mem_cache[index].meta_cache_entries;
  while(current_ptr!=NULL)
   {
    if (current_ptr->inode_num == this_inode) /* A hit */
     {
/*Lock body*/
/*TODO: May need to add checkpoint here so that long sem wait will free all locks*/
      sem_wait(&((current_ptr->cache_entry_body).access_sem));
      memcpy(returned_stat, &((current_ptr->cache_entry_body).this_stat),sizeof(struct stat));
      gettimeofday(&((current_ptr->cache_entry_body).last_access_time),NULL);
      sem_post(&((current_ptr->cache_entry_body).access_sem));

      sem_post(&(meta_mem_cache[index].header_sem));
      return 0;
     }
    current_ptr = current_ptr->next;
   }

  sem_post(&(meta_mem_cache[index].header_sem));
  return -1; /*Not found*/
 }

int meta_cache_fill_stat(ino_t this_inode, struct stat *updated_stat, char stat_dirty)
 {
  int index;
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr, *new_ptr;

  index = hash_inode_to_meta_cache(this_inode);
/*First lock corresponding header*/
  sem_wait(&(meta_mem_cache[index].header_sem));

  current_ptr = meta_mem_cache[index].meta_cache_entries;
  while(current_ptr!=NULL)
   {
    if (current_ptr->inode_num == this_inode) /* A hit */
     {
/*Lock body*/
/*TODO: May need to add checkpoint here so that long sem wait will free all locks*/
      sem_wait(&((current_ptr->cache_entry_body).access_sem));
      memcpy(&((current_ptr->cache_entry_body).this_stat), updated_stat,sizeof(struct stat));
      (current_ptr->cache_entry_body).stat_dirty = stat_dirty;
      gettimeofday(&((current_ptr->cache_entry_body).last_access_time),NULL);
      sem_post(&((current_ptr->cache_entry_body).access_sem));

      sem_post(&(meta_mem_cache[index].header_sem));
      return 0;
     }
    current_ptr = current_ptr->next;
   }

  new_ptr = malloc(sizeof(META_CACHE_LOOKUP_ENTRY_STRUCT));
  if (new_ptr==NULL)
   return -1;
  memset(new_ptr,0,sizeof(META_CACHE_LOOKUP_ENTRY_STRUCT));
  
  new_ptr->next = meta_mem_cache[index].meta_cache_entries;
  meta_mem_cache[index].meta_cache_entries = new_ptr;
  new_ptr->inode_num = this_inode;
  sem_init(&((new_ptr->cache_entry_body).access_sem),0,1);
  gettimeofday(&((new_ptr->cache_entry_body).last_access_time),NULL);
  memcpy(&((new_ptr->cache_entry_body).this_stat),updated_stat,sizeof(struct stat));
  (new_ptr->cache_entry_body).stat_dirty = stat_dirty;

  sem_post(&(meta_mem_cache[index].header_sem));
  return 0;
 }
