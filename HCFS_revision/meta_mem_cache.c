#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "params.h"
#include "fuseop.h"
#include "super_inode.h"
#include "meta_mem_cache.h"

/* TODO: cache meta file pointer and close only after some idle interval */

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
int meta_cache_push_dir_page(META_CACHE_ENTRY_STRUCT *body_ptr, long nextfilepos, mode_t child_mode, DIR_ENTRY_PAGE *temppage)
 {
  int ret_val;

  if (body_ptr->dir_entry_cache[0]==NULL)
   {
    body_ptr->dir_entry_cache[0] = malloc(sizeof(DIR_ENTRY_PAGE));
    memcpy((body_ptr->dir_entry_cache[0]), temppage,sizeof(DIR_ENTRY_PAGE));
    body_ptr->dir_entry_cache_pos[0] = nextfilepos;
    body_ptr->dir_entry_cache_dirty[0] = TRUE;
    body_ptr->dir_entry_cache_mode[0] = child_mode;
   }
  else
   {
    if (body_ptr->dir_entry_cache[1]==NULL)
     {
      body_ptr->dir_entry_cache_pos[1] = body_ptr->dir_entry_cache_pos[0];
      body_ptr->dir_entry_cache_dirty[1] = body_ptr->dir_entry_cache_dirty[0];
      body_ptr->dir_entry_cache[1] = body_ptr->dir_entry_cache[0];
      body_ptr->dir_entry_cache_mode[1] = body_ptr->dir_entry_cache_mode[0];
      body_ptr->dir_entry_cache[0] = malloc(sizeof(DIR_ENTRY_PAGE));
      memcpy((body_ptr->dir_entry_cache[0]), temppage,sizeof(DIR_ENTRY_PAGE));
      body_ptr->dir_entry_cache_pos[0] = nextfilepos;
      body_ptr->dir_entry_cache_dirty[0] = TRUE;
      body_ptr->dir_entry_cache_mode[0] = child_mode;
     }
    else
     {
      /* Need to flush first */
      ret_val = meta_cache_flush_dir_cache(body_ptr,1);
      free(body_ptr->dir_entry_cache[1]);
      body_ptr->dir_entry_cache_pos[1] = body_ptr->dir_entry_cache_pos[0];
      body_ptr->dir_entry_cache_dirty[1] = body_ptr->dir_entry_cache_dirty[0];
      body_ptr->dir_entry_cache[1] = body_ptr->dir_entry_cache[0];
      body_ptr->dir_entry_cache_mode[1] = body_ptr->dir_entry_cache_mode[0];
      body_ptr->dir_entry_cache[0] = malloc(sizeof(DIR_ENTRY_PAGE));
      memcpy((body_ptr->dir_entry_cache[0]), temppage,sizeof(DIR_ENTRY_PAGE));
      body_ptr->dir_entry_cache_pos[0] = nextfilepos;
      body_ptr->dir_entry_cache_dirty[0] = TRUE;
      body_ptr->dir_entry_cache_mode[0] = child_mode;
     }
   }
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

int meta_cache_flush_dir_cache(META_CACHE_ENTRY_STRUCT *body_ptr, int entry_index)
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

  fseek(fptr,body_ptr->dir_entry_cache_pos[entry_index],SEEK_SET);
  fwrite(body_ptr->dir_entry_cache[entry_index],sizeof(DIR_ENTRY_PAGE),1,fptr);

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

  if (S_ISREG((body_ptr->this_stat).st_mode) == TRUE)
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

  if (S_ISDIR((body_ptr->this_stat).st_mode) == TRUE)
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
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr;
  META_CACHE_ENTRY_STRUCT *body_ptr;
  char need_new;

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
    meta_mem_cache[index].num_entries++;
   }
/*Lock body*/
/*TODO: May need to add checkpoint here so that long sem wait will free all locks*/

  sem_wait(&((current_ptr->cache_entry_body).access_sem));

  if (inode_stat != NULL)
   {
    memcpy(&((current_ptr->cache_entry_body).this_stat),inode_stat,sizeof(struct stat));
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

  gettimeofday(&((current_ptr->cache_entry_body).last_access_time),NULL);
  sem_post(&((current_ptr->cache_entry_body).access_sem));

  if (current_ptr->something_dirty == FALSE)
   current_ptr->something_dirty = TRUE;

  if (META_CACHE_FLUSH_NOW == TRUE)
   ret_val = flush_single_meta_cache_entry(current_ptr);

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
    meta_mem_cache[index].num_entries++;
   }
/*Lock body*/
/*TODO: May need to add checkpoint here so that long sem wait will free all locks*/

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
  sem_post(&((current_ptr->cache_entry_body).access_sem));

  sem_post(&(meta_mem_cache[index].header_sem));

  return 0;
 }



int meta_cache_lookup_dir_data(ino_t this_inode, struct stat *inode_stat, DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page, long page_pos, mode_t child_mode)
 {
  int index;
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr;
  META_CACHE_ENTRY_STRUCT *body_ptr;
  char thismetapath[METAPATHLEN];
  FILE *fptr;
  char meta_opened,need_new;
  SUPER_INODE_ENTRY tempentry;
  int ret_code;

  printf("Debug meta cache lookup dir data page_pos %ld, dir_page %d\n",page_pos, dir_page);

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
    meta_mem_cache[index].num_entries++;
   }
/*Lock body*/
/*TODO: May need to add checkpoint here so that long sem wait will free all locks*/

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
          body_ptr->dir_entry_cache_mode[0] = child_mode;
          memcpy(dir_page, (body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE));
         }
        else
         {
          if (body_ptr->dir_entry_cache[1]==NULL)
           {
            body_ptr->dir_entry_cache_pos[1] = body_ptr->dir_entry_cache_pos[0];
            body_ptr->dir_entry_cache_dirty[1] = body_ptr->dir_entry_cache_dirty[0];
            body_ptr->dir_entry_cache[1] = body_ptr->dir_entry_cache[0];
            body_ptr->dir_entry_cache_mode[1] = body_ptr->dir_entry_cache_mode[0];


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
            body_ptr->dir_entry_cache_mode[0] = child_mode;
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
            body_ptr->dir_entry_cache_pos[1] = body_ptr->dir_entry_cache_pos[0];
            body_ptr->dir_entry_cache[0] = malloc(sizeof(DIR_ENTRY_PAGE));

            fseek(fptr,page_pos,SEEK_SET);
            fread((body_ptr->dir_entry_cache[0]),sizeof(DIR_ENTRY_PAGE),1,fptr);

            body_ptr->dir_entry_cache_pos[0] = page_pos;
            body_ptr->dir_entry_cache_mode[0] = child_mode;
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
  sem_post(&((current_ptr->cache_entry_body).access_sem));

  sem_post(&(meta_mem_cache[index].header_sem));

  printf("Debug meta cache lookup dir data cached pos %ld\n",body_ptr->dir_entry_cache_pos[0]);

  return 0;
 }

int meta_cache_update_dir_data(ino_t this_inode, struct stat *inode_stat, DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page, long page_pos, mode_t child_mode)
 {
  /*Always change dirty status to TRUE here as we always update*/
/*For dir entry page lookup or update, only allow one lookup/update at a time,
and will check page_pos input against the two entries in the cache. If does not match any
of the two, flush the older page entry first before processing the new one */

  int index;
  int ret_val;
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr;
  META_CACHE_ENTRY_STRUCT *body_ptr;
  char need_new;

  printf("Debug meta cache update dir data page_pos %ld\n",page_pos);

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
    meta_mem_cache[index].num_entries++;
   }
/*Lock body*/
/*TODO: May need to add checkpoint here so that long sem wait will free all locks*/

  sem_wait(&((current_ptr->cache_entry_body).access_sem));

  if (inode_stat != NULL)
   {
    memcpy(&((current_ptr->cache_entry_body).this_stat),inode_stat,sizeof(struct stat));
    (current_ptr->cache_entry_body).stat_dirty = TRUE;
   }

  body_ptr = &(current_ptr->cache_entry_body);
  if (dir_meta_ptr != NULL)
   {
    if (body_ptr->dir_meta == NULL)
     body_ptr->dir_meta = malloc(sizeof(DIR_META_TYPE));
    memcpy(((current_ptr->cache_entry_body).dir_meta), dir_meta_ptr,sizeof(DIR_META_TYPE));
    (current_ptr->cache_entry_body).meta_dirty = TRUE;
   }

  if (dir_page != NULL)
   {
    if ((body_ptr->dir_entry_cache[0]!=NULL) && (body_ptr->dir_entry_cache_pos[0]==page_pos))
     {
      memcpy((body_ptr->dir_entry_cache[0]), dir_page,sizeof(DIR_ENTRY_PAGE));
      body_ptr->dir_entry_cache_dirty[0] = TRUE;
     }
    else
     {
      if ((body_ptr->dir_entry_cache[1]!=NULL) && (body_ptr->dir_entry_cache_pos[1]==page_pos))
       {
        /* TODO: consider swapping entries 0 and 1 */
        memcpy((body_ptr->dir_entry_cache[1]), dir_page,sizeof(DIR_ENTRY_PAGE));
        body_ptr->dir_entry_cache_dirty[1] = TRUE;
       }
      else
       {
        /* Cannot find the requested page in cache */
        meta_cache_push_dir_page(body_ptr,page_pos,child_mode,dir_page);
       }
     }
   }

  gettimeofday(&((current_ptr->cache_entry_body).last_access_time),NULL);
  sem_post(&((current_ptr->cache_entry_body).access_sem));

  if (current_ptr->something_dirty == FALSE)
   current_ptr->something_dirty = TRUE;

  if (META_CACHE_FLUSH_NOW == TRUE)
   ret_val = flush_single_meta_cache_entry(current_ptr);

  printf("Debug meta cache update dir data cached pos %ld\n",body_ptr->dir_entry_cache_pos[0]);

  sem_post(&(meta_mem_cache[index].header_sem));
  return 0;
 }

int meta_cache_seek_empty_dir_entry(ino_t this_inode, DIR_ENTRY_PAGE *result_page,long *page_pos, mode_t child_mode, DIR_META_TYPE *result_meta)
 {
  FILE *fptr;
  char thismetapath[METAPATHLEN];
  struct stat inode_stat;
  DIR_META_TYPE dir_meta;
  DIR_ENTRY_PAGE temppage;
  int ret_items;
  int ret_val;
  long nextfilepos,oldfilepos;
  int index;
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr;
  META_CACHE_ENTRY_STRUCT *body_ptr;
  char need_new,meta_opened;
  int can_use_index;

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
    meta_mem_cache[index].num_entries++;
   }
/*Lock body*/
/*TODO: May need to add checkpoint here so that long sem wait will free all locks*/

  sem_wait(&((current_ptr->cache_entry_body).access_sem));

  body_ptr = &(current_ptr->cache_entry_body);

  meta_opened = FALSE;

/*First check if any of the two cached page entries contains empty entry and return
the one with smaller file pos */

  if (need_new == FALSE)
   {
    can_use_index = -1;
    if (body_ptr->dir_entry_cache[0]!=NULL)
     {
      if (((S_ISREG(body_ptr->dir_entry_cache_mode[0]) == TRUE) && (S_ISREG(child_mode) == TRUE)) ||
          ((S_ISDIR(body_ptr->dir_entry_cache_mode[0]) == TRUE) && (S_ISDIR(child_mode) == TRUE)))
       {
        if ((body_ptr->dir_entry_cache[0])->num_entries < MAX_DIR_ENTRIES_PER_PAGE)
         {
          *page_pos = body_ptr->dir_entry_cache_pos[0];
          can_use_index = 0;
         }
       }
     }
    if (body_ptr->dir_entry_cache[1]!=NULL)
     {
      if (((S_ISREG(body_ptr->dir_entry_cache_mode[1]) == TRUE) && (S_ISREG(child_mode) == TRUE)) ||
          ((S_ISDIR(body_ptr->dir_entry_cache_mode[1]) == TRUE) && (S_ISDIR(child_mode) == TRUE)))
       {
        if ((body_ptr->dir_entry_cache[1])->num_entries < MAX_DIR_ENTRIES_PER_PAGE)
         {
          if (((can_use_index >=0) && ((*page_pos) > body_ptr->dir_entry_cache_pos[1]))
            || (can_use_index < 0))
           {
            *page_pos = body_ptr->dir_entry_cache_pos[1];
            can_use_index = 1;
           }
         }
       }
     }

    if (can_use_index >=0)
     {
      memcpy(result_page,(body_ptr->dir_entry_cache[can_use_index]),sizeof(DIR_ENTRY_PAGE));

      gettimeofday(&((current_ptr->cache_entry_body).last_access_time),NULL);

      sem_post(&((current_ptr->cache_entry_body).access_sem));
      sem_post(&(meta_mem_cache[index].header_sem));

      return 0;
     }
   }

/* Cannot find the empty dir entry in any of the two cached page entries. Proceed to search from meta file */

  if (need_new == TRUE)
   {
    /*Open meta file and fetch stat from super inode*/
    fetch_meta_path(thismetapath,current_ptr->inode_num);

    fptr = fopen(thismetapath,"r+");
    if (fptr==NULL)
     goto file_exception;

    setbuf(fptr,NULL); 
    flock(fileno(fptr),LOCK_EX);
    meta_opened = TRUE;
    fseek(fptr,0,SEEK_SET);
    fread(&(body_ptr->this_stat),sizeof(struct stat),1,fptr);
   }

  if (body_ptr->dir_meta == NULL)
   {
    body_ptr->dir_meta = malloc(sizeof(DIR_META_TYPE));

    if (meta_opened == FALSE)
     {
      fetch_meta_path(thismetapath,current_ptr->inode_num);

      fptr = fopen(thismetapath,"r+");
      if (fptr==NULL)
       goto file_exception;

      setbuf(fptr,NULL); 
      flock(fileno(fptr),LOCK_EX);
      meta_opened = TRUE;
     }
    fseek(fptr,sizeof(struct stat), SEEK_SET);
    fread(body_ptr->dir_meta,sizeof(DIR_META_TYPE),1,fptr);
   }

  memcpy(&(dir_meta),body_ptr->dir_meta,sizeof(DIR_META_TYPE));

  if (child_mode & S_IFREG)
   {
    nextfilepos=dir_meta.next_file_page;
   }
  else
   {
    if (child_mode & S_IFDIR)
     nextfilepos=dir_meta.next_subdir_page;
   }
  memset(&temppage,0,sizeof(DIR_ENTRY_PAGE));

  while(nextfilepos!=0)
   {
    /*First check if the page is in the cache*/

    if ((body_ptr->dir_entry_cache[0]!=NULL) && (body_ptr->dir_entry_cache_pos[0] == nextfilepos))
     {
      /* Pretty sure there is no empty entry here, otherwise should have returned it */
      if ((body_ptr->dir_entry_cache[0])->next_page == 0)
       break;
      nextfilepos = (body_ptr->dir_entry_cache[0])->next_page;
     }
    else
     {
      if ((body_ptr->dir_entry_cache[1]!=NULL) && (body_ptr->dir_entry_cache_pos[1] == nextfilepos))
       {
        /* Pretty sure there is no empty entry here, otherwise should have returned it */
        if ((body_ptr->dir_entry_cache[1])->next_page == 0)
         break;
        nextfilepos = (body_ptr->dir_entry_cache[1])->next_page;
       }
      else
       {
        if (meta_opened == FALSE)
         {
          fetch_meta_path(thismetapath,current_ptr->inode_num);

          fptr = fopen(thismetapath,"r+");
          if (fptr==NULL)
           goto file_exception;

          setbuf(fptr,NULL); 
          flock(fileno(fptr),LOCK_EX);
          meta_opened = TRUE;
         }

        fseek(fptr,nextfilepos,SEEK_SET);
        if (ftell(fptr)!=nextfilepos)
         goto file_exception;
        ret_items = fread(&temppage,sizeof(DIR_ENTRY_PAGE),1,fptr);
        if (ret_items <1)
         goto file_exception;

        if (temppage.num_entries < MAX_DIR_ENTRIES_PER_PAGE)
         {   /*This page has empty dir entry. Fill it.*/
          /*TODO: First should allocate a page entry in cache for this*/

        /* Cannot find the requested page in cache */
          meta_cache_push_dir_page(body_ptr,nextfilepos,child_mode, &temppage);

          *page_pos = nextfilepos;
          memcpy(result_page,&temppage,sizeof(DIR_ENTRY_PAGE));

          if (meta_opened == TRUE)
           {
            flock(fptr,LOCK_UN);
            fclose(fptr);
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
       }
     }

    if (temppage.next_page == 0)
     break;
    nextfilepos = temppage.next_page;
   }

  /*Allocate a new page*/

  if (meta_opened == FALSE)
   {
    fetch_meta_path(thismetapath,current_ptr->inode_num);

    fptr = fopen(thismetapath,"r+");
    if (fptr==NULL)
     goto file_exception;

    setbuf(fptr,NULL); 
    flock(fileno(fptr),LOCK_EX);
    meta_opened = TRUE;
   }

  oldfilepos = nextfilepos;

  fseek(fptr,0,SEEK_END);
  nextfilepos=ftell(fptr);

  if (oldfilepos!=0)
   {
    if ((body_ptr->dir_entry_cache[0]!=NULL) && (body_ptr->dir_entry_cache_pos[0] == oldfilepos))
     {
      /* Pretty sure there is no empty entry here, otherwise should have returned it */
      (body_ptr->dir_entry_cache[0])->next_page = nextfilepos;
      body_ptr->dir_entry_cache_dirty[0] = TRUE;
     }
    else
     {
      if ((body_ptr->dir_entry_cache[1]!=NULL) && (body_ptr->dir_entry_cache_pos[1] == oldfilepos))
       {
        (body_ptr->dir_entry_cache[1])->next_page = nextfilepos;
        body_ptr->dir_entry_cache_dirty[1] = TRUE;
       }
      else
       {
        temppage.next_page = nextfilepos;
        fseek(fptr,oldfilepos,SEEK_SET);
        ret_items = fwrite(&temppage,sizeof(DIR_ENTRY_PAGE),1,fptr);
        if (ret_items <1)
         goto file_exception;
       }
     }
   }
  else
   {
    if (child_mode & S_IFREG)
     {
      (body_ptr->dir_meta)->next_file_page=nextfilepos;
     }
    else
     {
      if (child_mode & S_IFDIR)
       {
        (body_ptr->dir_meta)->next_subdir_page=nextfilepos;
       }
     }
    body_ptr->meta_dirty = TRUE;
   }

  fseek(fptr,nextfilepos,SEEK_SET);
  memset(&temppage,0,sizeof(DIR_ENTRY_PAGE));
  temppage.num_entries=0;
  temppage.next_page=0;
  fwrite(&temppage,sizeof(DIR_ENTRY_PAGE),1,fptr);

  meta_cache_push_dir_page(body_ptr,nextfilepos,child_mode,&temppage);

  *page_pos = nextfilepos;
  memcpy(result_page,&temppage,sizeof(DIR_ENTRY_PAGE));
  memcpy(result_meta,body_ptr->dir_meta,sizeof(DIR_META_TYPE));

  if (meta_opened == TRUE)
   {
    flock(fptr,LOCK_UN);
    fclose(fptr);
   }

  gettimeofday(&((current_ptr->cache_entry_body).last_access_time),NULL);

  sem_post(&((current_ptr->cache_entry_body).access_sem));

  if (current_ptr->something_dirty == FALSE)
   current_ptr->something_dirty = TRUE;

  if (META_CACHE_FLUSH_NOW == TRUE)
   ret_val = flush_single_meta_cache_entry(current_ptr);

  sem_post(&(meta_mem_cache[index].header_sem));
  return 0;

/* Exception handling from here */
file_exception:

  sem_post(&((current_ptr->cache_entry_body).access_sem));
  sem_post(&(meta_mem_cache[index].header_sem));
  if (meta_opened == TRUE)
   {
    flock(fptr,LOCK_UN);
    fclose(fptr);
   }
  return -1;


 }

/*Returns 0 and negative entry_index if not found. Returns negative numbers if
exception.*/
int meta_cache_seek_dir_entry(ino_t this_inode, DIR_ENTRY_PAGE *result_page,long *page_pos, int *entry_index, char *childname, mode_t child_mode)
 {
  FILE *fptr;
  char thismetapath[METAPATHLEN];
  struct stat inode_stat;
  DIR_META_TYPE dir_meta;
  DIR_ENTRY_PAGE temppage;
  DIR_ENTRY_PAGE *tmp_page_ptr;
  int ret_items;
  int ret_val;
  long nextfilepos,oldfilepos;
  int index;
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr;
  META_CACHE_ENTRY_STRUCT *body_ptr;
  char need_new,meta_opened;
  int can_use_index;
  int count;

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
   }
/*Lock body*/
/*TODO: May need to add checkpoint here so that long sem wait will free all locks*/

  sem_wait(&((current_ptr->cache_entry_body).access_sem));

  body_ptr = &(current_ptr->cache_entry_body);

  meta_opened = FALSE;

/*First check if any of the two cached page entries contains the target entry*/

  if (need_new == FALSE)
   {
    can_use_index = -1;
    if (body_ptr->dir_entry_cache[0]!=NULL)
     {
      if (((S_ISREG(body_ptr->dir_entry_cache_mode[0]) == TRUE) && (S_ISREG(child_mode) == TRUE)) ||
          ((S_ISDIR(body_ptr->dir_entry_cache_mode[0]) == TRUE) && (S_ISDIR(child_mode) == TRUE)))
       {
        tmp_page_ptr = body_ptr->dir_entry_cache[0];
        for(count=0;count<tmp_page_ptr->num_entries;count++)
         {
          if (strcmp(tmp_page_ptr->dir_entries[count].d_name,childname)==0)
           {
            *entry_index=count;
            *page_pos = body_ptr->dir_entry_cache_pos[0];
            can_use_index = 0;
           }
         }
       }
     }

    if ((can_use_index < 0) && (body_ptr->dir_entry_cache[1]!=NULL))
     {
      if (((S_ISREG(body_ptr->dir_entry_cache_mode[1]) == TRUE) && (S_ISREG(child_mode) == TRUE)) ||
          ((S_ISDIR(body_ptr->dir_entry_cache_mode[1]) == TRUE) && (S_ISDIR(child_mode) == TRUE)))
       {
        tmp_page_ptr = body_ptr->dir_entry_cache[1];
        for(count=0;count<tmp_page_ptr->num_entries;count++)
         {
          if (strcmp(tmp_page_ptr->dir_entries[count].d_name,childname)==0)
           {
            *entry_index=count;
            *page_pos = body_ptr->dir_entry_cache_pos[1];
            can_use_index = 1;
           }
         }
       }
     }

    if (can_use_index >=0)
     {
      memcpy(result_page,(body_ptr->dir_entry_cache[can_use_index]),sizeof(DIR_ENTRY_PAGE));

      gettimeofday(&((current_ptr->cache_entry_body).last_access_time),NULL);

      sem_post(&((current_ptr->cache_entry_body).access_sem));
      sem_post(&(meta_mem_cache[index].header_sem));

      return 0;
     }
   }

/* Cannot find the empty dir entry in any of the two cached page entries. Proceed to search from meta file */

  if (need_new == TRUE)
   {
    /*Open meta file and fetch stat from super inode*/
    fetch_meta_path(thismetapath,current_ptr->inode_num);

    fptr = fopen(thismetapath,"r+");
    if (fptr==NULL)
     goto file_exception;

    setbuf(fptr,NULL); 
    flock(fileno(fptr),LOCK_EX);
    meta_opened = TRUE;
    fseek(fptr,0,SEEK_SET);
    fread(&(body_ptr->this_stat),sizeof(struct stat),1,fptr);
   }

  if (body_ptr->dir_meta == NULL)
   {
    body_ptr->dir_meta = malloc(sizeof(DIR_META_TYPE));

    if (meta_opened == FALSE)
     {
      fetch_meta_path(thismetapath,current_ptr->inode_num);

      fptr = fopen(thismetapath,"r+");
      if (fptr==NULL)
       goto file_exception;

      setbuf(fptr,NULL); 
      flock(fileno(fptr),LOCK_EX);
      meta_opened = TRUE;
     }
    fseek(fptr,sizeof(struct stat), SEEK_SET);
    fread(body_ptr->dir_meta,sizeof(DIR_META_TYPE),1,fptr);
   }

  memcpy(&(dir_meta),body_ptr->dir_meta,sizeof(DIR_META_TYPE));

  if (child_mode & S_IFREG)
   {
    nextfilepos=dir_meta.next_file_page;
   }
  else
   {
    if (child_mode & S_IFDIR)
     nextfilepos=dir_meta.next_subdir_page;
   }
  memset(&temppage,0,sizeof(DIR_ENTRY_PAGE));

  while(nextfilepos!=0)
   {
    /*First check if the page is in the cache*/

    if ((body_ptr->dir_entry_cache[0]!=NULL) && (body_ptr->dir_entry_cache_pos[0] == nextfilepos))
     {
      /* Pretty sure there is no such entry here, otherwise should have returned it */
      if ((body_ptr->dir_entry_cache[0])->next_page == 0)
       break;
      nextfilepos = (body_ptr->dir_entry_cache[0])->next_page;
     }
    else
     {
      if ((body_ptr->dir_entry_cache[1]!=NULL) && (body_ptr->dir_entry_cache_pos[1] == nextfilepos))
       {
        /* Pretty sure there is no such entry here, otherwise should have returned it */
        if ((body_ptr->dir_entry_cache[1])->next_page == 0)
         break;
        nextfilepos = (body_ptr->dir_entry_cache[1])->next_page;
       }
      else
       {
        if (meta_opened == FALSE)
         {
          fetch_meta_path(thismetapath,current_ptr->inode_num);

          fptr = fopen(thismetapath,"r+");
          if (fptr==NULL)
           goto file_exception;

          setbuf(fptr,NULL); 
          flock(fileno(fptr),LOCK_EX);
          meta_opened = TRUE;
         }

        fseek(fptr,nextfilepos,SEEK_SET);
        if (ftell(fptr)!=nextfilepos)
         goto file_exception;
        ret_items = fread(&temppage,sizeof(DIR_ENTRY_PAGE),1,fptr);
        if (ret_items <1)
         goto file_exception;

        for(count=0;count<temppage.num_entries;count++)
         {
          if (strcmp(temppage.dir_entries[count].d_name,childname)==0)
           {
            /*Found the entry */
            meta_cache_push_dir_page(body_ptr,nextfilepos,child_mode,&temppage);

            *page_pos = nextfilepos;
            memcpy(result_page,&temppage,sizeof(DIR_ENTRY_PAGE));
            *entry_index = count;

            if (meta_opened == TRUE)
             {
              flock(fptr,LOCK_UN);
              fclose(fptr);
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
         }
       }
     }

    if (temppage.next_page == 0)
     break;
    nextfilepos = temppage.next_page;
   }

  /*Entry not found. Do nothing and return false*/

  if (meta_opened == TRUE)
   {
    flock(fptr,LOCK_UN);
    fclose(fptr);
   }

  gettimeofday(&((current_ptr->cache_entry_body).last_access_time),NULL);

  sem_post(&((current_ptr->cache_entry_body).access_sem));

  if (current_ptr->something_dirty == FALSE)
   current_ptr->something_dirty = TRUE;

  if (META_CACHE_FLUSH_NOW == TRUE)
   ret_val = flush_single_meta_cache_entry(current_ptr);

  sem_post(&(meta_mem_cache[index].header_sem));
  *entry_index = -1;
  return 0;

/* Exception handling from here */
file_exception:

  sem_post(&((current_ptr->cache_entry_body).access_sem));
  sem_post(&(meta_mem_cache[index].header_sem));
  if (meta_opened == TRUE)
   {
    flock(fptr,LOCK_UN);
    fclose(fptr);
   }
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
