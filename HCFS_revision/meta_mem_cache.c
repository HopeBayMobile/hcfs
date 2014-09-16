#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "params.h"
#include "fuseop.h"
#include "super_inode.h"
#include "meta_mem_cache.h"
#include "dir_entry_btree.h"

/* TODO: cache meta file pointer and close only after some idle interval */

META_CACHE_HEADER_STRUCT *meta_mem_cache;

int meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
 {
  char thismetapath[METAPATHLEN];

  fetch_meta_path(thismetapath,body_ptr->inode_num);

  body_ptr->fptr = fopen(thismetapath,"r+");
  if (body_ptr->fptr==NULL)
   {
    if (access(thismetapath,F_OK)<0)
     body_ptr->fptr = fopen(thismetapath,"w+");  /*File may not exist*/
    if (body_ptr->fptr == NULL)
     {
      return -1;
     }
   }

  setbuf(body_ptr->fptr,NULL); 
  flock(fileno(body_ptr->fptr),LOCK_EX);
  body_ptr->meta_opened = TRUE;
  return 0;
 }

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
int meta_cache_push_dir_page(META_CACHE_ENTRY_STRUCT *body_ptr, DIR_ENTRY_PAGE *temppage)
 {
  int ret_val;

  if (body_ptr->dir_entry_cache[0]==NULL)
   {
    body_ptr->dir_entry_cache[0] = malloc(sizeof(DIR_ENTRY_PAGE));
    memcpy((body_ptr->dir_entry_cache[0]), temppage,sizeof(DIR_ENTRY_PAGE));
    body_ptr->dir_entry_cache_dirty[0] = TRUE;
   }
  else
   {
    if (body_ptr->dir_entry_cache[1]==NULL)
     {
      body_ptr->dir_entry_cache_dirty[1] = body_ptr->dir_entry_cache_dirty[0];
      body_ptr->dir_entry_cache[1] = body_ptr->dir_entry_cache[0];
      body_ptr->dir_entry_cache[0] = malloc(sizeof(DIR_ENTRY_PAGE));
      memcpy((body_ptr->dir_entry_cache[0]), temppage,sizeof(DIR_ENTRY_PAGE));
      body_ptr->dir_entry_cache_dirty[0] = TRUE;
     }
    else
     {
      /* Need to flush first */
      ret_val = meta_cache_flush_dir_cache(body_ptr,1);
      free(body_ptr->dir_entry_cache[1]);
      body_ptr->dir_entry_cache_dirty[1] = body_ptr->dir_entry_cache_dirty[0];
      body_ptr->dir_entry_cache[1] = body_ptr->dir_entry_cache[0];
      body_ptr->dir_entry_cache[0] = malloc(sizeof(DIR_ENTRY_PAGE));
      memcpy((body_ptr->dir_entry_cache[0]), temppage,sizeof(DIR_ENTRY_PAGE));
      body_ptr->dir_entry_cache_dirty[0] = TRUE;
     }
   }
  return 0;
 }

int meta_cache_flush_block_cache(META_CACHE_ENTRY_STRUCT *body_ptr, int entry_index)
 {
  int ret_val;
  /*Assume meta cache entry access sem is already locked*/

  if (body_ptr->meta_opened == FALSE)
   {
    ret_val = meta_cache_open_file(body_ptr);
    if (ret_val < 0)
     return ret_val;
   }

  fseek(body_ptr->fptr,body_ptr->block_entry_cache_pos[entry_index],SEEK_SET);
  fwrite(body_ptr->block_entry_cache[entry_index],sizeof(BLOCK_ENTRY_PAGE),1,body_ptr->fptr);

  super_inode_mark_dirty((body_ptr->this_stat).st_ino);

  return 0;
 }

int meta_cache_flush_dir_cache(META_CACHE_ENTRY_STRUCT *body_ptr, int entry_index)
 {
  int ret_val;
  /*Assume meta cache entry access sem is already locked*/

  if (body_ptr->meta_opened == FALSE)
   {
    ret_val = meta_cache_open_file(body_ptr);
    if (ret_val < 0)
     return ret_val;
   }

  fseek(body_ptr->fptr,(body_ptr->dir_entry_cache[entry_index])->this_page_pos,SEEK_SET);
  fwrite(body_ptr->dir_entry_cache[entry_index],sizeof(DIR_ENTRY_PAGE),1,body_ptr->fptr);

  super_inode_mark_dirty((body_ptr->this_stat).st_ino);

  return 0;
 }


int flush_single_meta_cache_entry(META_CACHE_ENTRY_STRUCT *body_ptr)
 {
  SUPER_INODE_ENTRY tempentry;
  int ret_val;
  int ret_code;
  int sem_val;

  sem_getvalue(&(body_ptr->access_sem), &sem_val);
  if (sem_val > 0)
   {
    /*Not locked, return -1*/
    return -1;
   }

  if (body_ptr->something_dirty == FALSE)
   {
    return 0;
   }

  if (body_ptr->meta_opened == FALSE)
   {
    ret_val = meta_cache_open_file(body_ptr);
    if (ret_val < 0)
     return ret_val;
   }

  if (body_ptr->stat_dirty == TRUE)
   {
    fseek(body_ptr->fptr,0,SEEK_SET);
    fwrite(&(body_ptr->this_stat),sizeof(struct stat),1,body_ptr->fptr);
    body_ptr->stat_dirty = FALSE;
   }

  if (S_ISREG((body_ptr->this_stat).st_mode) == TRUE)
   {
    if (body_ptr->meta_dirty == TRUE)
     {
      fseek(body_ptr->fptr,sizeof(struct stat),SEEK_SET);
      fwrite((body_ptr->file_meta),sizeof(FILE_META_TYPE),1,body_ptr->fptr);
      body_ptr->meta_dirty = FALSE;
     }
    if ((body_ptr->block_entry_cache_dirty[0] == TRUE) && (body_ptr->block_entry_cache[0] != NULL))
     {
      fseek(body_ptr->fptr,body_ptr->block_entry_cache_pos[0],SEEK_SET);
      fwrite(body_ptr->block_entry_cache[0],sizeof(BLOCK_ENTRY_PAGE),1,body_ptr->fptr);
      body_ptr->block_entry_cache_dirty[0] = FALSE;
     }
    if ((body_ptr->block_entry_cache_dirty[1] == TRUE) && (body_ptr->block_entry_cache[1] != NULL))
     {
      fseek(body_ptr->fptr,body_ptr->block_entry_cache_pos[1],SEEK_SET);
      fwrite(body_ptr->block_entry_cache[1],sizeof(BLOCK_ENTRY_PAGE),1,body_ptr->fptr);
      body_ptr->block_entry_cache_dirty[1] = FALSE;
     }
   }

  if (S_ISDIR((body_ptr->this_stat).st_mode) == TRUE)
   {
    if (body_ptr->meta_dirty == TRUE)
     {
      fseek(body_ptr->fptr,sizeof(struct stat),SEEK_SET);
      fwrite((body_ptr->dir_meta),sizeof(DIR_META_TYPE),1,body_ptr->fptr);
      body_ptr->meta_dirty = FALSE;
     }
    if ((body_ptr->dir_entry_cache_dirty[0] == TRUE) && (body_ptr->dir_entry_cache[0] != NULL))
     {
      fseek(body_ptr->fptr,(body_ptr->dir_entry_cache[0])->this_page_pos,SEEK_SET);
      fwrite(body_ptr->dir_entry_cache[0],sizeof(DIR_ENTRY_PAGE),1,body_ptr->fptr);
      body_ptr->dir_entry_cache_dirty[0] = FALSE;
     }
    if ((body_ptr->dir_entry_cache_dirty[1] == TRUE) && (body_ptr->dir_entry_cache[1] != NULL))
     {
      fseek(body_ptr->fptr,(body_ptr->dir_entry_cache[1])->this_page_pos,SEEK_SET);
      fwrite(body_ptr->dir_entry_cache[1],sizeof(DIR_ENTRY_PAGE),1,body_ptr->fptr);
      body_ptr->dir_entry_cache_dirty[1] = FALSE;
     }
   }
   
  /*TODO: Add flush of xattr pages here */

  /*Update stat info in super inode no matter what so that meta file got pushed to cloud*/
  /*TODO: May need to simply this so that only dirty status in super inode is updated */
  super_inode_update_stat(body_ptr->inode_num, &(body_ptr->this_stat));

  body_ptr->something_dirty = FALSE;

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
        sem_wait(&((current_ptr->cache_entry_body).access_sem));
        if ((current_ptr->cache_entry_body).something_dirty == TRUE)
         {
          flush_single_meta_cache_entry(&(current_ptr->cache_entry_body));
         }
        if ((current_ptr->cache_entry_body).meta_opened == TRUE)
         {
          flock((current_ptr->cache_entry_body).fptr,LOCK_UN);
          fclose((current_ptr->cache_entry_body).fptr);
          (current_ptr->cache_entry_body).meta_opened = FALSE;
         }

        free_single_meta_cache_entry(current_ptr);
        old_ptr = current_ptr;
        current_ptr = current_ptr->next;
        sem_post(&((old_ptr->cache_entry_body).access_sem));
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

  if (entry_body->dir_meta != NULL)
   free(entry_body->dir_meta);
  if (entry_body->file_meta != NULL)
   free(entry_body->file_meta);

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

int meta_cache_update_file_data(ino_t this_inode, struct stat *inode_stat, FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page, long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
 {
  /*Always change dirty status to TRUE here as we always update*/
/*For block entry page lookup or update, only allow one lookup/update at a time,
and will check page_pos input against the two entries in the cache. If does not match any
of the two, flush the older page entry first before processing the new one */

  int index;
  int ret_val, sem_val;
  char need_new;

  sem_getvalue(&(body_ptr->access_sem), &sem_val);
  if (sem_val > 0)
   {
    /*Not locked, return -1*/
    return -1;
   }

  if (inode_stat != NULL)
   {
    memcpy(&(body_ptr->this_stat),inode_stat,sizeof(struct stat));
    body_ptr->stat_dirty = TRUE;
   }

  if (file_meta_ptr != NULL)
   {
    if (body_ptr->file_meta == NULL)
     body_ptr->file_meta = malloc(sizeof(FILE_META_TYPE));
    memcpy((body_ptr->file_meta), file_meta_ptr,sizeof(FILE_META_TYPE));
    body_ptr->meta_dirty = TRUE;
   }

  if (block_page != NULL)
   {
    if ((body_ptr->block_entry_cache[0]!=NULL) && (body_ptr->block_entry_cache_pos[0]==page_pos))
     {
      memcpy((body_ptr->block_entry_cache[0]), block_page,sizeof(BLOCK_ENTRY_PAGE));
      body_ptr->block_entry_cache_dirty[0] = TRUE;
     }
    else
     {
      if ((body_ptr->block_entry_cache[1]!=NULL) && (body_ptr->block_entry_cache_pos[1]==page_pos))
       {
        /* TODO: consider swapping entries 0 and 1 */
        memcpy((body_ptr->block_entry_cache[1]), block_page,sizeof(BLOCK_ENTRY_PAGE));
        body_ptr->block_entry_cache_dirty[1] = TRUE;
       }
      else
       {
        /* Cannot find the requested page in cache */
        if (body_ptr->block_entry_cache[0]==NULL)
         {
          body_ptr->block_entry_cache[0] = malloc(sizeof(BLOCK_ENTRY_PAGE));
          memcpy((body_ptr->block_entry_cache[0]), block_page,sizeof(BLOCK_ENTRY_PAGE));
          body_ptr->block_entry_cache_pos[0] = page_pos;
          body_ptr->block_entry_cache_dirty[0] = TRUE;
         }
        else
         {
          if (body_ptr->block_entry_cache[1]==NULL)
           {
            body_ptr->block_entry_cache_pos[1] = body_ptr->block_entry_cache_pos[0];
            body_ptr->block_entry_cache_dirty[1] = body_ptr->block_entry_cache_dirty[0];
            body_ptr->block_entry_cache[1] = body_ptr->block_entry_cache[0];
            body_ptr->block_entry_cache[0] = malloc(sizeof(BLOCK_ENTRY_PAGE));
            body_ptr->block_entry_cache_pos[0] = page_pos;
            memcpy((body_ptr->block_entry_cache[0]), block_page,sizeof(BLOCK_ENTRY_PAGE));
            body_ptr->block_entry_cache_dirty[0] = TRUE;
           }
          else
           {
            /* Need to flush first */
            ret_val = meta_cache_flush_block_cache(body_ptr,1);
            free(body_ptr->block_entry_cache[1]);
            body_ptr->block_entry_cache_pos[1] = body_ptr->block_entry_cache_pos[0];
            body_ptr->block_entry_cache_dirty[1] = body_ptr->block_entry_cache_dirty[0];
            body_ptr->block_entry_cache[1] = body_ptr->block_entry_cache[0];
            body_ptr->block_entry_cache[0] = malloc(sizeof(BLOCK_ENTRY_PAGE));
            memcpy((body_ptr->block_entry_cache[0]), block_page,sizeof(BLOCK_ENTRY_PAGE));
            body_ptr->block_entry_cache_pos[0] = page_pos;
            body_ptr->block_entry_cache_dirty[0] = TRUE;
           }
         }
       }
     }
   }

  gettimeofday(&(body_ptr->last_access_time),NULL);

  if (body_ptr->something_dirty == FALSE)
   body_ptr->something_dirty = TRUE;

  return 0;
 }

int meta_cache_lookup_file_data(ino_t this_inode, struct stat *inode_stat, FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page, long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
 {
  int index;
  char thismetapath[METAPATHLEN];
  SUPER_INODE_ENTRY tempentry;
  int ret_code, ret_val, sem_val;

  sem_getvalue(&(body_ptr->access_sem), &sem_val);
  if (sem_val > 0)
   {
    /*Not locked, return -1*/
    return -1;
   }

  if (inode_stat!=NULL)
   memcpy(inode_stat, &(body_ptr->this_stat),sizeof(struct stat));

  if (file_meta_ptr != NULL)
   {
    if (body_ptr->file_meta == NULL)
     {
      body_ptr->file_meta = malloc(sizeof(FILE_META_TYPE));

      if (body_ptr->meta_opened == FALSE)
       {
        ret_val = meta_cache_open_file(body_ptr);
        if (ret_val < 0)
         goto file_exception;
       }

      fseek(body_ptr->fptr,sizeof(struct stat), SEEK_SET);
      fread(body_ptr->file_meta,sizeof(FILE_META_TYPE),1,body_ptr->fptr);
     }

    memcpy(file_meta_ptr,body_ptr->file_meta,sizeof(FILE_META_TYPE));
   }

  if (block_page != NULL)
   {
    if ((body_ptr->block_entry_cache[0]!=NULL) && (body_ptr->block_entry_cache_pos[0]==page_pos))
     {
      memcpy(block_page,(body_ptr->block_entry_cache[0]),sizeof(BLOCK_ENTRY_PAGE));
     }
    else
     {
      if ((body_ptr->block_entry_cache[1]!=NULL) && (body_ptr->block_entry_cache_pos[1]==page_pos))
       {
        /* TODO: consider swapping entries 0 and 1 */
        memcpy(block_page,(body_ptr->block_entry_cache[1]),sizeof(BLOCK_ENTRY_PAGE));
       }
      else
       {
        /* Cannot find the requested page in cache */
        if (body_ptr->block_entry_cache[0]==NULL)
         {
          body_ptr->block_entry_cache[0] = malloc(sizeof(BLOCK_ENTRY_PAGE));

          if (body_ptr->meta_opened == FALSE)
           {
            ret_val = meta_cache_open_file(body_ptr);
            if (ret_val < 0)
             goto file_exception;
           }

          fseek(body_ptr->fptr,page_pos,SEEK_SET);
          fread((body_ptr->block_entry_cache[0]),sizeof(BLOCK_ENTRY_PAGE),1,body_ptr->fptr);

          body_ptr->block_entry_cache_pos[0] = page_pos;
          memcpy(block_page, (body_ptr->block_entry_cache[0]),sizeof(BLOCK_ENTRY_PAGE));
         }
        else
         {
          if (body_ptr->block_entry_cache[1]==NULL)
           {
            body_ptr->block_entry_cache_pos[1] = body_ptr->block_entry_cache_pos[0];
            body_ptr->block_entry_cache_dirty[1] = body_ptr->block_entry_cache_dirty[0];
            body_ptr->block_entry_cache[1] = body_ptr->block_entry_cache[0];


            body_ptr->block_entry_cache[0] = malloc(sizeof(BLOCK_ENTRY_PAGE));

            if (body_ptr->meta_opened == FALSE)
             {
              ret_val = meta_cache_open_file(body_ptr);
              if (ret_val < 0)
               goto file_exception;
             }

            fseek(body_ptr->fptr,page_pos,SEEK_SET);
            fread((body_ptr->block_entry_cache[0]),sizeof(BLOCK_ENTRY_PAGE),1,body_ptr->fptr);

            body_ptr->block_entry_cache_pos[0] = page_pos;
            memcpy(block_page, (body_ptr->block_entry_cache[0]),sizeof(BLOCK_ENTRY_PAGE));
           }
          else
           {
            if (body_ptr->meta_opened == FALSE)
             {
              ret_val = meta_cache_open_file(body_ptr);
              if (ret_val < 0)
               goto file_exception;
             }

           /* Need to flush first */
            fseek(body_ptr->fptr,body_ptr->block_entry_cache_pos[1],SEEK_SET);
            fwrite(body_ptr->block_entry_cache[1],sizeof(BLOCK_ENTRY_PAGE),1,body_ptr->fptr);
            free(body_ptr->block_entry_cache[1]);
            super_inode_mark_dirty((body_ptr->this_stat).st_ino);

            body_ptr->block_entry_cache_pos[1] = body_ptr->block_entry_cache_pos[0];
            body_ptr->block_entry_cache_dirty[1] = body_ptr->block_entry_cache_dirty[0];
            body_ptr->block_entry_cache[1] = body_ptr->block_entry_cache[0];
            body_ptr->block_entry_cache[0] = malloc(sizeof(BLOCK_ENTRY_PAGE));

            fseek(body_ptr->fptr,page_pos,SEEK_SET);
            fread((body_ptr->block_entry_cache[0]),sizeof(BLOCK_ENTRY_PAGE),1,body_ptr->fptr);

            body_ptr->block_entry_cache_pos[0] = page_pos;
            memcpy(block_page, (body_ptr->block_entry_cache[0]),sizeof(BLOCK_ENTRY_PAGE));
           }
         }
       }
     }
   }

  gettimeofday(&(body_ptr->last_access_time),NULL);

  return 0;

/* Exception handling from here */
file_exception:
  return -1;

 }



int meta_cache_lookup_dir_data(ino_t this_inode, struct stat *inode_stat, DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page, META_CACHE_ENTRY_STRUCT *body_ptr)
 {
  int index;
  SUPER_INODE_ENTRY tempentry;
  int ret_code, ret_val, sem_val;

  sem_getvalue(&(body_ptr->access_sem), &sem_val);
  if (sem_val > 0)
   {
    /*Not locked, return -1*/
    return -1;
   }

  if (inode_stat!=NULL)
   memcpy(inode_stat, &(body_ptr->this_stat),sizeof(struct stat));

  if (dir_meta_ptr != NULL)
   {
    if (body_ptr->dir_meta == NULL)
     {
      body_ptr->dir_meta = malloc(sizeof(DIR_META_TYPE));

      if (body_ptr->meta_opened == FALSE)
       {
        ret_val = meta_cache_open_file(body_ptr);
        if (ret_val < 0)
         return -1;
       }

      fseek(body_ptr->fptr,sizeof(struct stat), SEEK_SET);
      fread(body_ptr->dir_meta,sizeof(DIR_META_TYPE),1,body_ptr->fptr);
     }

    memcpy(dir_meta_ptr,body_ptr->dir_meta,sizeof(DIR_META_TYPE));
   }

  if (dir_page != NULL)
   {
    if ((body_ptr->dir_entry_cache[0]!=NULL) && ((body_ptr->dir_entry_cache[0])->this_page_pos==dir_page->this_page_pos))
     {
      memcpy(dir_page,(body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE));
     }
    else
     {
      if ((body_ptr->dir_entry_cache[1]!=NULL) && ((body_ptr->dir_entry_cache[1])->this_page_pos==dir_page->this_page_pos))
       {
        /* TODO: consider swapping entries 0 and 1 */
        memcpy(dir_page,(body_ptr->dir_entry_cache[1]),sizeof(DIR_ENTRY_PAGE));
       }
      else
       {
        /* Cannot find the requested page in cache */
        if (body_ptr->dir_entry_cache[0]==NULL)
         {
          body_ptr->dir_entry_cache[0] = malloc(sizeof(DIR_ENTRY_PAGE));
          if (body_ptr->meta_opened == FALSE)
           {
            ret_val = meta_cache_open_file(body_ptr);
            if (ret_val < 0)
             return -1;
           }

          fseek(body_ptr->fptr,dir_page->this_page_pos,SEEK_SET);
          fread((body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE),1,body_ptr->fptr);

          memcpy(dir_page, (body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE));
         }
        else
         {
          if (body_ptr->dir_entry_cache[1]==NULL)
           {
            body_ptr->dir_entry_cache_dirty[1] = body_ptr->dir_entry_cache_dirty[0];
            body_ptr->dir_entry_cache[1] = body_ptr->dir_entry_cache[0];

            body_ptr->dir_entry_cache[0] = malloc(sizeof(DIR_ENTRY_PAGE));
            if (body_ptr->meta_opened == FALSE)
             {
              ret_val = meta_cache_open_file(body_ptr);
              if (ret_val < 0)
               return -1;
             }

            fseek(body_ptr->fptr,dir_page->this_page_pos,SEEK_SET);
            fread((body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE),1,body_ptr->fptr);

            memcpy(dir_page, (body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE));
           }
          else
           {
            if (body_ptr->meta_opened == FALSE)
             {
              ret_val = meta_cache_open_file(body_ptr);
              if (ret_val < 0)
               return -1;
             }

           /* Need to flush first */
            fseek(body_ptr->fptr,(body_ptr->dir_entry_cache[1])->this_page_pos,SEEK_SET);
            fwrite(body_ptr->dir_entry_cache[1],sizeof(DIR_ENTRY_PAGE),1,body_ptr->fptr);
            free(body_ptr->dir_entry_cache[1]);
            super_inode_mark_dirty((body_ptr->this_stat).st_ino);

            body_ptr->dir_entry_cache_dirty[1] = body_ptr->dir_entry_cache_dirty[0];
            body_ptr->dir_entry_cache[1] = body_ptr->dir_entry_cache[0];
            body_ptr->dir_entry_cache[0] = malloc(sizeof(DIR_ENTRY_PAGE));

            fseek(body_ptr->fptr,dir_page->this_page_pos,SEEK_SET);
            fread((body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE),1,body_ptr->fptr);

            memcpy(dir_page, (body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE));
           }
         }
       }
     }
   }

  gettimeofday(&(body_ptr->last_access_time),NULL);


  return 0;
 }

int meta_cache_update_dir_data(ino_t this_inode, struct stat *inode_stat, DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page, META_CACHE_ENTRY_STRUCT *body_ptr)
 {
  /*Always change dirty status to TRUE here as we always update*/
/*For dir entry page lookup or update, only allow one lookup/update at a time,
and will check page_pos input against the two entries in the cache. If does not match any
of the two, flush the older page entry first before processing the new one */

  int index;
  int ret_val, sem_val;
  char need_new;

  printf("Debug meta cache update dir data\n");

  sem_getvalue(&(body_ptr->access_sem), &sem_val);
  if (sem_val > 0)
   {
    /*Not locked, return -1*/
    return -1;
   }

  if (inode_stat != NULL)
   {
    memcpy(&(body_ptr->this_stat),inode_stat,sizeof(struct stat));
    body_ptr->stat_dirty = TRUE;
   }

  if (dir_meta_ptr != NULL)
   {
    if (body_ptr->dir_meta == NULL)
     body_ptr->dir_meta = malloc(sizeof(DIR_META_TYPE));
    memcpy((body_ptr->dir_meta), dir_meta_ptr,sizeof(DIR_META_TYPE));
    body_ptr->meta_dirty = TRUE;
   }

  if (dir_page != NULL)
   {
    if ((body_ptr->dir_entry_cache[0]!=NULL) && ((body_ptr->dir_entry_cache[0])->this_page_pos==dir_page->this_page_pos))
     {
      memcpy((body_ptr->dir_entry_cache[0]), dir_page,sizeof(DIR_ENTRY_PAGE));
      body_ptr->dir_entry_cache_dirty[0] = TRUE;
     }
    else
     {
      if ((body_ptr->dir_entry_cache[1]!=NULL) && ((body_ptr->dir_entry_cache[1])->this_page_pos==dir_page->this_page_pos))
       {
        /* TODO: consider swapping entries 0 and 1 */
        memcpy((body_ptr->dir_entry_cache[1]), dir_page,sizeof(DIR_ENTRY_PAGE));
        body_ptr->dir_entry_cache_dirty[1] = TRUE;
       }
      else
       {
        /* Cannot find the requested page in cache */
        meta_cache_push_dir_page(body_ptr, dir_page);
       }
     }
   }

  gettimeofday(&(body_ptr->last_access_time),NULL);


  if (body_ptr->something_dirty == FALSE)
   body_ptr->something_dirty = TRUE;

  return 0;
 }


/*Returns 0 and result entry with d_ino = 0 if not found. Returns negative numbers if
exception.*/
int meta_cache_seek_dir_entry(ino_t this_inode, DIR_ENTRY_PAGE *result_page, int *result_index, char *childname, META_CACHE_ENTRY_STRUCT *body_ptr)
 {
  FILE *fptr;
  char thismetapath[METAPATHLEN];
  struct stat inode_stat;
  DIR_META_TYPE dir_meta;
  DIR_ENTRY_PAGE temppage, rootpage, tmp_resultpage;
  DIR_ENTRY_PAGE *tmp_page_ptr;
  int ret_items;
  int ret_val;
  long nextfilepos,oldfilepos;
  int index;
  int can_use_index;
  int count, sem_val;
  DIR_ENTRY tmp_entry;
  int tmp_index;

  sem_getvalue(&(body_ptr->access_sem), &sem_val);
  if (sem_val > 0)
   {
    /*Not locked, return -1*/
    return -1;
   }

/*First check if any of the two cached page entries contains the target entry*/

  strcpy(tmp_entry.d_name, childname);

  can_use_index = -1;
  if (body_ptr->dir_entry_cache[0]!=NULL)
   {
    tmp_page_ptr = body_ptr->dir_entry_cache[0];
    ret_val = dentry_binary_search(tmp_page_ptr->dir_entries, tmp_page_ptr->num_entries, &tmp_entry, &tmp_index);
    if (ret_val >=0)
     {
      *result_index = ret_val;
      memcpy(result_page, tmp_page_ptr, sizeof(DIR_ENTRY_PAGE));
      can_use_index = 0;
     }
   }

  if ((can_use_index < 0) && (body_ptr->dir_entry_cache[1]!=NULL))
   {
    tmp_page_ptr = body_ptr->dir_entry_cache[1];
    ret_val = dentry_binary_search(tmp_page_ptr->dir_entries, tmp_page_ptr->num_entries, &tmp_entry, &tmp_index);
    if (ret_val >=0)
     {
      *result_index = ret_val;
      memcpy(result_page, tmp_page_ptr, sizeof(DIR_ENTRY_PAGE));
      can_use_index = 1;
     }
   }

  if (can_use_index >=0)
   {
    gettimeofday(&(body_ptr->last_access_time),NULL);

    return 0;
   }

/* Cannot find the empty dir entry in any of the two cached page entries. Proceed to search from meta file */

  if (body_ptr->dir_meta == NULL)
   {
    body_ptr->dir_meta = malloc(sizeof(DIR_META_TYPE));

    if (body_ptr->meta_opened == FALSE)
     {
      fetch_meta_path(thismetapath,body_ptr->inode_num);

      body_ptr->fptr = fopen(thismetapath,"r+");
      if (body_ptr->fptr==NULL)
       goto file_exception;

      setbuf(body_ptr->fptr,NULL); 
      flock(fileno(body_ptr->fptr),LOCK_EX);
      body_ptr->meta_opened = TRUE;
     }
    fseek(body_ptr->fptr,sizeof(struct stat), SEEK_SET);
    fread(body_ptr->dir_meta,sizeof(DIR_META_TYPE),1,body_ptr->fptr);
   }

  memcpy(&(dir_meta),body_ptr->dir_meta,sizeof(DIR_META_TYPE));

  nextfilepos=dir_meta.root_entry_page;
  if (nextfilepos <= 0)
   {
    /* Nothing in the dir. Return 0 */
    return 0;
   }

  memset(&temppage,0,sizeof(DIR_ENTRY_PAGE));
  memset(&rootpage,0,sizeof(DIR_ENTRY_PAGE));

  if (body_ptr->meta_opened == FALSE)
   {
    fetch_meta_path(thismetapath,body_ptr->inode_num);

    body_ptr->fptr = fopen(thismetapath,"r+");
    if (body_ptr->fptr==NULL)
     goto file_exception;

    setbuf(body_ptr->fptr,NULL); 
    flock(fileno(body_ptr->fptr),LOCK_EX);
    body_ptr->meta_opened = TRUE;
   }
  fseek(body_ptr->fptr,nextfilepos, SEEK_SET);
  fread(&rootpage, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr); /*Read the root node*/

  ret_val = search_dir_entry_btree(childname, &rootpage, body_ptr->fptr, &tmp_index, &tmp_resultpage);

  gettimeofday(&(body_ptr->last_access_time),NULL);

  if (body_ptr->something_dirty == FALSE)
   body_ptr->something_dirty = TRUE;

  if (ret_val < 0) /*Not found*/
   {
    *result_index = -1;
    return 0;
   }
  /*Found the entry */
  meta_cache_push_dir_page(body_ptr,&tmp_resultpage);
  *result_index = tmp_index;
  memcpy(result_page, &tmp_resultpage, sizeof(DIR_ENTRY_PAGE));

  return 0;

/* Exception handling from here */
file_exception:
  return -1;

 }

int meta_cache_remove(ino_t this_inode)
 {
  FILE *fptr;
  int ret_val;
  int index;
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr, *prev_ptr;
  META_CACHE_ENTRY_STRUCT *body_ptr;
  char need_new,meta_opened;
  int can_use_index;

  index = hash_inode_to_meta_cache(this_inode);
/*First lock corresponding header*/
  sem_wait(&(meta_mem_cache[index].header_sem));

  current_ptr = meta_mem_cache[index].meta_cache_entries;
  prev_ptr = NULL;
  need_new = TRUE;
  while(current_ptr!=NULL)
   {
    if (current_ptr->inode_num == this_inode) /* A hit */
     {
      need_new = FALSE;
      break;
     }
    prev_ptr = current_ptr;
    current_ptr = current_ptr->next;
   }

  if (need_new == TRUE) /*If did not find cache entry*/
   {
    sem_post(&(meta_mem_cache[index].header_sem));
    return 0;
   }

/*Lock body*/
/*TODO: May need to add checkpoint here so that long sem wait will free all locks*/

  sem_wait(&((current_ptr->cache_entry_body).access_sem));

  body_ptr = &(current_ptr->cache_entry_body);

  if (body_ptr->dir_meta !=NULL)
   free(body_ptr->dir_meta);

  if (body_ptr->file_meta !=NULL)
   free(body_ptr->file_meta);

  if (body_ptr->dir_entry_cache[0] !=NULL)
   free(body_ptr->dir_entry_cache[0]);

  if (body_ptr->dir_entry_cache[1] !=NULL)
   free(body_ptr->dir_entry_cache[1]);

  if (body_ptr->block_entry_cache[0] !=NULL)
   free(body_ptr->block_entry_cache[0]);

  if (body_ptr->block_entry_cache[1] !=NULL)
   free(body_ptr->block_entry_cache[1]);

  current_ptr->inode_num = 0;

  sem_post(&((current_ptr->cache_entry_body).access_sem));

  if (prev_ptr !=NULL)
   {
    prev_ptr->next = current_ptr->next;
   }
  else
   {
    meta_mem_cache[index].meta_cache_entries = current_ptr->next;
   }

  meta_mem_cache[index].num_entries--;

  free(current_ptr);

  sem_post(&(meta_mem_cache[index].header_sem));

  return 0;
 }

META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
 {
  int ret_val;
  int index;
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr;
  META_CACHE_ENTRY_STRUCT *body_ptr;
  char need_new;
  int can_use_index;
  int count;
  SUPER_INODE_ENTRY tempentry;
  META_CACHE_ENTRY_STRUCT *result_ptr;

  index = hash_inode_to_meta_cache(this_inode);
/*First lock corresponding header*/
  sem_wait(&(meta_mem_cache[index].header_sem));

  current_ptr = meta_mem_cache[index].meta_cache_entries;
  need_new = TRUE;
  while(current_ptr!=NULL)
   {
    if (current_ptr->inode_num == this_inode) /* A hit */
     {
      need_new = FALSE;
      break;
     }
    current_ptr = current_ptr->next;
   }

  if (need_new == TRUE)
   {
    current_ptr = malloc(sizeof(META_CACHE_LOOKUP_ENTRY_STRUCT));
    if (current_ptr==NULL)
     return -EACCES;
    memset(current_ptr,0,sizeof(META_CACHE_LOOKUP_ENTRY_STRUCT));
  
    current_ptr->next = meta_mem_cache[index].meta_cache_entries;
    meta_mem_cache[index].meta_cache_entries = current_ptr;
    current_ptr->inode_num = this_inode;
    sem_init(&((current_ptr->cache_entry_body).access_sem),0,1);
    meta_mem_cache[index].num_entries++;
    ret_val =super_inode_read(this_inode, &tempentry);
    (current_ptr->cache_entry_body).inode_num = this_inode;
    (current_ptr->cache_entry_body).meta_opened = FALSE;
    memcpy(&((current_ptr->cache_entry_body).this_stat),&(tempentry.inode_stat),sizeof(struct stat));

   }
/*Lock body*/
/*TODO: May need to add checkpoint here so that long sem wait will free all locks*/
  result_ptr = &(current_ptr->cache_entry_body);

  sem_wait(&((current_ptr->cache_entry_body).access_sem));
  sem_post(&(meta_mem_cache[index].header_sem));

  return result_ptr;
 }

int meta_cache_unlock_entry(ino_t this_inode, META_CACHE_ENTRY_STRUCT *target_ptr)
 {
  int ret_val;
  int sem_val;

  sem_getvalue(&(target_ptr->access_sem), &sem_val);
  if (sem_val > 0)
   {
    /*Not locked, return -1*/
    return -1;
   }

  gettimeofday(&(target_ptr->last_access_time),NULL);

  if (META_CACHE_FLUSH_NOW == TRUE)
   ret_val = flush_single_meta_cache_entry(target_ptr);

  if (target_ptr->meta_opened == TRUE)
   {
    flock(target_ptr->fptr,LOCK_UN);
    fclose(target_ptr->fptr);
    target_ptr->meta_opened = FALSE;
   }

  sem_post(&(target_ptr->access_sem));

  return 0;
 }

int meta_cache_drop_pages(META_CACHE_ENTRY_STRUCT *body_ptr)
 {
  int ret_val;
  int sem_val;

  sem_getvalue(&(body_ptr->access_sem), &sem_val);
  if (sem_val > 0)
   {
    /*Not locked, return -1*/
    return -1;
   }

  gettimeofday(&(body_ptr->last_access_time),NULL);

  if (body_ptr->dir_entry_cache[0]!=NULL)
   {
    if (body_ptr->dir_entry_cache_dirty[0]==TRUE)
     ret_val = meta_cache_flush_dir_cache(body_ptr,0);
    free(body_ptr->dir_entry_cache[0]);
    body_ptr->dir_entry_cache[0] = NULL;
    body_ptr->dir_entry_cache_dirty[0] = FALSE;
   }

  if (body_ptr->dir_entry_cache[1]!=NULL)
   {
    if (body_ptr->dir_entry_cache_dirty[1]==TRUE)
     ret_val = meta_cache_flush_dir_cache(body_ptr,1);
    free(body_ptr->dir_entry_cache[1]);
    body_ptr->dir_entry_cache[1] = NULL;
    body_ptr->dir_entry_cache_dirty[1] = FALSE;
   }

  return 0;
 }
