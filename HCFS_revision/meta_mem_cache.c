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
int meta_cache_flush_block_cache(META_CACHE_ENTRY_STRUCT *body_ptr, int entry_index)
 {
  /*Assume meta cache entry access sem is already locked*/
  char thismetapath[METAPATHLEN];
  FILE *fptr;

  fetch_meta_path(thismetapath,(body_ptr->this_stat).st_ino);

  fptr = fopen(thismetapath,"r+");
  if (fptr==NULL)
   {
    if (access(thismetapath,F_OK)<0)
     fptr = fopen(thismetapath,"w+");  /*File may not exist*/
    if (fptr == NULL)
     {
      return -1;
     }
   }
  setbuf(fptr,NULL);
  
  flock(fileno(fptr),LOCK_EX);

  fseek(fptr,body_ptr->block_entry_cache_pos[entry_index],SEEK_SET);
  fwrite(body_ptr->block_entry_cache[entry_index],sizeof(BLOCK_ENTRY_PAGE),1,fptr);

  flock(fileno(fptr),LOCK_UN);
  fclose(fptr);

  super_inode_mark_dirty((body_ptr->this_stat).st_ino);

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
    if (access(thismetapath,F_OK)<0)
     fptr = fopen(thismetapath,"w+");  /*File may not exist*/
    if (fptr == NULL)
     {
      sem_post(&((entry_ptr->cache_entry_body).access_sem));
      return -ENOENT;
     }
   }
  setbuf(fptr,NULL);
  
  flock(fileno(fptr),LOCK_EX);

  if (body_ptr->stat_dirty == TRUE)
   {
    fseek(fptr,0,SEEK_SET);
    fwrite(&(body_ptr->this_stat),sizeof(struct stat),1,fptr);
    body_ptr->stat_dirty = FALSE;
   }

  if (S_ISREG(body_ptr->inode_mode) == TRUE)
   {
    if (body_ptr->meta_dirty == TRUE)
     {
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fwrite((body_ptr->file_meta),sizeof(FILE_META_TYPE),1,fptr);
      body_ptr->meta_dirty = FALSE;
     }
    if ((body_ptr->block_entry_cache_dirty[0] == TRUE) && (body_ptr->block_entry_cache[0] != NULL))
     {
      fseek(fptr,body_ptr->block_entry_cache_pos[0],SEEK_SET);
      fwrite(body_ptr->block_entry_cache[0],sizeof(BLOCK_ENTRY_PAGE),1,fptr);
      body_ptr->block_entry_cache_dirty[0] = FALSE;
     }
    if ((body_ptr->block_entry_cache_dirty[1] == TRUE) && (body_ptr->block_entry_cache[1] != NULL))
     {
      fseek(fptr,body_ptr->block_entry_cache_pos[1],SEEK_SET);
      fwrite(body_ptr->block_entry_cache[1],sizeof(BLOCK_ENTRY_PAGE),1,fptr);
      body_ptr->block_entry_cache_dirty[1] = FALSE;
     }
   }

  if (S_ISDIR(body_ptr->inode_mode) == TRUE)
   {
    if (body_ptr->meta_dirty == TRUE)
     {
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fwrite((body_ptr->dir_meta),sizeof(DIR_META_TYPE),1,fptr);
      body_ptr->meta_dirty = FALSE;
     }
    if ((body_ptr->dir_entry_cache_dirty[0] == TRUE) && (body_ptr->dir_entry_cache[0] != NULL))
     {
      fseek(fptr,body_ptr->dir_entry_cache_pos[0],SEEK_SET);
      fwrite(body_ptr->dir_entry_cache[0],sizeof(DIR_ENTRY_PAGE),1,fptr);
      body_ptr->dir_entry_cache_dirty[0] = FALSE;
     }
    if ((body_ptr->dir_entry_cache_dirty[1] == TRUE) && (body_ptr->dir_entry_cache[1] != NULL))
     {
      fseek(fptr,body_ptr->dir_entry_cache_pos[1],SEEK_SET);
      fwrite(body_ptr->dir_entry_cache[1],sizeof(DIR_ENTRY_PAGE),1,fptr);
      body_ptr->dir_entry_cache_dirty[1] = FALSE;
     }
   }
   
  /*TODO: Add flush of xattr pages here */

  flock(fileno(fptr),LOCK_UN);
  fclose(fptr);
  /*Update stat info in super inode no matter what so that meta file got pushed to cloud*/
  /*TODO: May need to simply this so that only dirty status in super inode is updated */
  super_inode_update_stat(entry_ptr->inode_num, &(body_ptr->this_stat));

  entry_ptr->something_dirty = FALSE;
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

int meta_cache_update_file_data(ino_t this_inode, struct stat *inode_stat, FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page, long page_pos)
 {
  /*Always change dirty status to TRUE here as we always update*/
/*For block entry page lookup or update, only allow one lookup/update at a time,
and will check page_pos input against the two entries in the cache. If does not match any
of the two, flush the older page entry first before processing the new one */

  int index;
  int ret_val;
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr, *new_ptr;
  META_CACHE_ENTRY_STRUCT *body_ptr;

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
      if (inode_stat != NULL)
       {
        memcpy(&((current_ptr->cache_entry_body).this_stat), inode_stat,sizeof(struct stat));
        (current_ptr->cache_entry_body).stat_dirty = TRUE;
       }
      body_ptr = &(current_ptr->cache_entry_body);
      if (file_meta_ptr != NULL)
       {
        if (body_ptr->file_meta == NULL)
         body_ptr->file_meta = malloc(sizeof(FILE_META_TYPE));
        memcpy(((current_ptr->cache_entry_body).file_meta), file_meta_ptr,sizeof(FILE_META_TYPE));
        (current_ptr->cache_entry_body).meta_dirty = TRUE;
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
                body_ptr->block_entry_cache_dirty[0] = TRUE;
               }
             }
           }
         }
       }

      gettimeofday(&((current_ptr->cache_entry_body).last_access_time),NULL);
      sem_post(&((current_ptr->cache_entry_body).access_sem));
      if (current_ptr->something_dirty == FALSE)
       current_ptr->something_dirty = TRUE;

      if (META_CACHE_FLUSH_NOW == TRUE)
       ret_val = flush_single_meta_cache_entry(current_ptr);

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

  if (inode_stat != NULL)
   {
    memcpy(&((new_ptr->cache_entry_body).this_stat),inode_stat,sizeof(struct stat));
    (new_ptr->cache_entry_body).stat_dirty = TRUE;
   }

  body_ptr = &(new_ptr->cache_entry_body);
  if (file_meta_ptr != NULL)
   {
    if (body_ptr->file_meta == NULL)
     body_ptr->file_meta = malloc(sizeof(FILE_META_TYPE));
    memcpy(((new_ptr->cache_entry_body).file_meta), file_meta_ptr,sizeof(FILE_META_TYPE));
    (new_ptr->cache_entry_body).meta_dirty = TRUE;
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

  if (new_ptr->something_dirty == FALSE)
   new_ptr->something_dirty = TRUE;

  if (META_CACHE_FLUSH_NOW == TRUE)
   ret_val = flush_single_meta_cache_entry(new_ptr);

  sem_post(&(meta_mem_cache[index].header_sem));
  return 0;
 }

int meta_cache_lookup_file_data(ino_t this_inode, struct stat *inode_stat, FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page, long page_pos)
 {
  int index;
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr;
  META_CACHE_ENTRY_STRUCT *body_ptr;
  char thismetapath[METAPATHLEN];
  FILE *fptr;
  char meta_opened,need_new;
  SUPER_INODE_ENTRY tempentry;
  int ret_code;

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
     return -1;
    memset(current_ptr,0,sizeof(META_CACHE_LOOKUP_ENTRY_STRUCT));
  
    current_ptr->next = meta_mem_cache[index].meta_cache_entries;
    meta_mem_cache[index].meta_cache_entries = current_ptr;
    current_ptr->inode_num = this_inode;
    sem_init(&((current_ptr->cache_entry_body).access_sem),0,1);
   }
/*Lock body*/
/*TODO: May need to add checkpoint here so that long sem wait will free all locks*/
  else
   sem_wait(&((current_ptr->cache_entry_body).access_sem));

  body_ptr = &(current_ptr->cache_entry_body);

  meta_opened = FALSE;

  if (need_new == TRUE)
   {
    if ((file_meta_ptr == NULL) && (block_page == NULL))
     {
      /*Fetch stat from super inode */
      ret_code =super_inode_read(this_inode, &tempentry);
      memcpy(&(body_ptr->this_stat),&(tempentry.inode_stat),sizeof(struct stat));
     }
    else
     {
      /*Open meta file and fetch stat from super inode*/
      fetch_meta_path(thismetapath,current_ptr->inode_num);

      fptr = fopen(thismetapath,"r+");
      if (fptr==NULL)
       {
        sem_post(&((current_ptr->cache_entry_body).access_sem));
        sem_post(&(meta_mem_cache[index].header_sem));
        return -1;
       }
      setbuf(fptr,NULL); 
      flock(fileno(fptr),LOCK_EX);
      meta_opened = TRUE;
      fseek(fptr,0,SEEK_SET);
      fread(&(body_ptr->this_stat),sizeof(struct stat),1,fptr);
     }
   }

  if (inode_stat!=NULL)
   memcpy(inode_stat, &(body_ptr->this_stat),sizeof(struct stat));

  if (file_meta_ptr != NULL)
   {
    if (body_ptr->file_meta == NULL)
     {
      body_ptr->file_meta = malloc(sizeof(FILE_META_TYPE));

      if (meta_opened == FALSE)
       {
        fetch_meta_path(thismetapath,current_ptr->inode_num);

        fptr = fopen(thismetapath,"r+");
        if (fptr==NULL)
         {
          sem_post(&((current_ptr->cache_entry_body).access_sem));
          sem_post(&(meta_mem_cache[index].header_sem));
          return -1;
         }
        setbuf(fptr,NULL); 
        flock(fileno(fptr),LOCK_EX);
        meta_opened = TRUE;
       }
      fseek(fptr,sizeof(struct stat), SEEK_SET);
      fread(body_ptr->file_meta,sizeof(FILE_META_TYPE),1,fptr);
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
          if (meta_opened == FALSE)
           {
            fetch_meta_path(thismetapath,current_ptr->inode_num);

            fptr = fopen(thismetapath,"r+");
            if (fptr==NULL)
             {
              sem_post(&((current_ptr->cache_entry_body).access_sem));
              sem_post(&(meta_mem_cache[index].header_sem));
              return -1;
             }
            setbuf(fptr,NULL); 
            flock(fileno(fptr),LOCK_EX);
            meta_opened = TRUE;
           }
          fseek(fptr,page_pos,SEEK_SET);
          fread((body_ptr->block_entry_cache[0]),sizeof(BLOCK_ENTRY_PAGE),1,fptr);

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

            if (meta_opened == FALSE)
             {
              fetch_meta_path(thismetapath,current_ptr->inode_num);

              fptr = fopen(thismetapath,"r+");
              if (fptr==NULL)
               {
                sem_post(&((current_ptr->cache_entry_body).access_sem));
                sem_post(&(meta_mem_cache[index].header_sem));
                return -1;
               }
              setbuf(fptr,NULL); 
              flock(fileno(fptr),LOCK_EX);

              meta_opened = TRUE;
             }
            fseek(fptr,page_pos,SEEK_SET);
            fread((body_ptr->block_entry_cache[0]),sizeof(BLOCK_ENTRY_PAGE),1,fptr);

            body_ptr->block_entry_cache_pos[0] = page_pos;
            memcpy(block_page, (body_ptr->block_entry_cache[0]),sizeof(BLOCK_ENTRY_PAGE));
           }
          else
           {
            if (meta_opened == FALSE)
             {
              fetch_meta_path(thismetapath,current_ptr->inode_num);

              fptr = fopen(thismetapath,"r+");
              if (fptr==NULL)
               {
                sem_post(&((current_ptr->cache_entry_body).access_sem));
                sem_post(&(meta_mem_cache[index].header_sem));
                return -1;
               }
              setbuf(fptr,NULL);
              flock(fileno(fptr),LOCK_EX);
              meta_opened = TRUE;
             }

           /* Need to flush first */
            fseek(fptr,body_ptr->block_entry_cache_pos[1],SEEK_SET);
            fwrite(body_ptr->block_entry_cache[1],sizeof(BLOCK_ENTRY_PAGE),1,fptr);
            free(body_ptr->block_entry_cache[1]);

            body_ptr->block_entry_cache_pos[1] = body_ptr->block_entry_cache_pos[0];
            body_ptr->block_entry_cache_dirty[1] = body_ptr->block_entry_cache_dirty[0];
            body_ptr->block_entry_cache[1] = body_ptr->block_entry_cache[0];
            body_ptr->block_entry_cache[0] = malloc(sizeof(BLOCK_ENTRY_PAGE));

            fseek(fptr,page_pos,SEEK_SET);
            fread((body_ptr->block_entry_cache[0]),sizeof(BLOCK_ENTRY_PAGE),1,fptr);

            body_ptr->block_entry_cache_pos[0] = page_pos;
            memcpy(block_page, (body_ptr->block_entry_cache[0]),sizeof(BLOCK_ENTRY_PAGE));
           }
         }
       }
     }
   }

  if (meta_opened == TRUE)
   {
    flock(fileno(fptr),LOCK_UN);
    fclose(fptr);
   }

  gettimeofday(&((current_ptr->cache_entry_body).last_access_time),NULL);
  if (need_new == FALSE)
   sem_post(&((current_ptr->cache_entry_body).access_sem));

  sem_post(&(meta_mem_cache[index].header_sem));

  return 0;
 }



int meta_cache_lookup_dir_data(ino_t this_inode, struct stat *inode_stat, DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page, long page_pos)
 {
  int index;
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr;
  META_CACHE_ENTRY_STRUCT *body_ptr;
  char thismetapath[METAPATHLEN];
  FILE *fptr;
  char meta_opened,need_new;
  SUPER_INODE_ENTRY tempentry;
  int ret_code;

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
     return -1;
    memset(current_ptr,0,sizeof(META_CACHE_LOOKUP_ENTRY_STRUCT));
  
    current_ptr->next = meta_mem_cache[index].meta_cache_entries;
    meta_mem_cache[index].meta_cache_entries = current_ptr;
    current_ptr->inode_num = this_inode;
    sem_init(&((current_ptr->cache_entry_body).access_sem),0,1);
   }
/*Lock body*/
/*TODO: May need to add checkpoint here so that long sem wait will free all locks*/
  else
   sem_wait(&((current_ptr->cache_entry_body).access_sem));

  body_ptr = &(current_ptr->cache_entry_body);

  meta_opened = FALSE;

  if (need_new == TRUE)
   {
    if ((dir_meta_ptr == NULL) && (dir_page == NULL))
     {
      /*Fetch stat from super inode */
      ret_code =super_inode_read(this_inode, &tempentry);
      memcpy(&(body_ptr->this_stat),&(tempentry.inode_stat),sizeof(struct stat));
     }
    else
     {
      /*Open meta file and fetch stat from super inode*/
      fetch_meta_path(thismetapath,current_ptr->inode_num);

      fptr = fopen(thismetapath,"r+");
      if (fptr==NULL)
       {
        sem_post(&((current_ptr->cache_entry_body).access_sem));
        sem_post(&(meta_mem_cache[index].header_sem));
        return -1;
       }
      setbuf(fptr,NULL); 
      flock(fileno(fptr),LOCK_EX);
      meta_opened = TRUE;
      fseek(fptr,0,SEEK_SET);
      fread(&(body_ptr->this_stat),sizeof(struct stat),1,fptr);
     }
   }

  if (inode_stat!=NULL)
   memcpy(inode_stat, &(body_ptr->this_stat),sizeof(struct stat));

  if (dir_meta_ptr != NULL)
   {
    if (body_ptr->dir_meta == NULL)
     {
      body_ptr->dir_meta = malloc(sizeof(DIR_META_TYPE));

      if (meta_opened == FALSE)
       {
        fetch_meta_path(thismetapath,current_ptr->inode_num);

        fptr = fopen(thismetapath,"r+");
        if (fptr==NULL)
         {
          sem_post(&((current_ptr->cache_entry_body).access_sem));
          sem_post(&(meta_mem_cache[index].header_sem));
          return -1;
         }
        setbuf(fptr,NULL); 
        flock(fileno(fptr),LOCK_EX);
        meta_opened = TRUE;
       }
      fseek(fptr,sizeof(struct stat), SEEK_SET);
      fread(body_ptr->dir_meta,sizeof(DIR_META_TYPE),1,fptr);
     }

    memcpy(dir_meta_ptr,body_ptr->dir_meta,sizeof(DIR_META_TYPE));
   }

  if (dir_page != NULL)
   {
    if ((body_ptr->dir_entry_cache[0]!=NULL) && (body_ptr->dir_entry_cache_pos[0]==page_pos))
     {
      memcpy(dir_page,(body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE));
     }
    else
     {
      if ((body_ptr->dir_entry_cache[1]!=NULL) && (body_ptr->dir_entry_cache_pos[1]==page_pos))
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
          if (meta_opened == FALSE)
           {
            fetch_meta_path(thismetapath,current_ptr->inode_num);

            fptr = fopen(thismetapath,"r+");
            if (fptr==NULL)
             {
              sem_post(&((current_ptr->cache_entry_body).access_sem));
              sem_post(&(meta_mem_cache[index].header_sem));
              return -1;
             }
            setbuf(fptr,NULL); 
            flock(fileno(fptr),LOCK_EX);
            meta_opened = TRUE;
           }
          fseek(fptr,page_pos,SEEK_SET);
          fread((body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE),1,fptr);

          body_ptr->dir_entry_cache_pos[0] = page_pos;
          memcpy(dir_page, (body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE));
         }
        else
         {
          if (body_ptr->dir_entry_cache[1]==NULL)
           {
            body_ptr->dir_entry_cache_pos[1] = body_ptr->dir_entry_cache_pos[0];
            body_ptr->dir_entry_cache_dirty[1] = body_ptr->dir_entry_cache_dirty[0];
            body_ptr->dir_entry_cache[1] = body_ptr->dir_entry_cache[0];


            body_ptr->dir_entry_cache[0] = malloc(sizeof(DIR_ENTRY_PAGE));

            if (meta_opened == FALSE)
             {
              fetch_meta_path(thismetapath,current_ptr->inode_num);

              fptr = fopen(thismetapath,"r+");
              if (fptr==NULL)
               {
                sem_post(&((current_ptr->cache_entry_body).access_sem));
                sem_post(&(meta_mem_cache[index].header_sem));
                return -1;
               }
              setbuf(fptr,NULL); 
              flock(fileno(fptr),LOCK_EX);

              meta_opened = TRUE;
             }
            fseek(fptr,page_pos,SEEK_SET);
            fread((body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE),1,fptr);

            body_ptr->dir_entry_cache_pos[0] = page_pos;
            memcpy(dir_page, (body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE));
           }
          else
           {
            if (meta_opened == FALSE)
             {
              fetch_meta_path(thismetapath,current_ptr->inode_num);

              fptr = fopen(thismetapath,"r+");
              if (fptr==NULL)
               {
                sem_post(&((current_ptr->cache_entry_body).access_sem));
                sem_post(&(meta_mem_cache[index].header_sem));
                return -1;
               }
              setbuf(fptr,NULL);
              flock(fileno(fptr),LOCK_EX);
              meta_opened = TRUE;
             }

           /* Need to flush first */
            fseek(fptr,body_ptr->dir_entry_cache_pos[1],SEEK_SET);
            fwrite(body_ptr->dir_entry_cache[1],sizeof(DIR_ENTRY_PAGE),1,fptr);
            free(body_ptr->dir_entry_cache[1]);

            body_ptr->dir_entry_cache_pos[1] = body_ptr->dir_entry_cache_pos[0];
            body_ptr->dir_entry_cache_dirty[1] = body_ptr->dir_entry_cache_dirty[0];
            body_ptr->dir_entry_cache[1] = body_ptr->dir_entry_cache[0];
            body_ptr->dir_entry_cache[0] = malloc(sizeof(DIR_ENTRY_PAGE));

            fseek(fptr,page_pos,SEEK_SET);
            fread((body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE),1,fptr);

            body_ptr->dir_entry_cache_pos[0] = page_pos;
            memcpy(dir_page, (body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE));
           }
         }
       }
     }
   }

  if (meta_opened == TRUE)
   {
    flock(fileno(fptr),LOCK_UN);
    fclose(fptr);
   }

  gettimeofday(&((current_ptr->cache_entry_body).last_access_time),NULL);
  if (need_new == FALSE)
   sem_post(&((current_ptr->cache_entry_body).access_sem));

  sem_post(&(meta_mem_cache[index].header_sem));

  return 0;
 }

