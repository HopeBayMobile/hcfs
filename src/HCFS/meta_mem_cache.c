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
#include <errno.h>

#include "global.h"
#include "params.h"
#include "fuseop.h"
#include "super_block.h"
#include "meta_mem_cache.h"
#include "dir_entry_btree.h"

/* TODO: cache meta file pointer and close only after some idle interval */
/* TODO: delayed write to disk */
/* TODO: Convert meta IO here and other files to allow segmented meta files */

META_CACHE_HEADER_STRUCT *meta_mem_cache;
sem_t num_entry_sem;
long current_meta_mem_cache_entries;

int meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
 {
  char thismetapath[METAPATHLEN];

  if (body_ptr->meta_opened == FALSE)
   {
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
   }
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

  current_meta_mem_cache_entries = 0;

  sem_init(&num_entry_sem,0,1);
  for(count=0;count<NUM_META_MEM_CACHE_HEADERS;count++)
   {
    ret_val = sem_init(&(meta_mem_cache[count].header_sem),0,1);
    meta_mem_cache[count].last_entry = NULL;
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

  current_meta_mem_cache_entries = 0;

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
      body_ptr->dir_entry_cache[1] = malloc(sizeof(DIR_ENTRY_PAGE));
      body_ptr->dir_entry_cache_dirty[1] = body_ptr->dir_entry_cache_dirty[0];
      memcpy(body_ptr->dir_entry_cache[1], body_ptr->dir_entry_cache[0], sizeof(DIR_ENTRY_PAGE));
      memcpy((body_ptr->dir_entry_cache[0]), temppage,sizeof(DIR_ENTRY_PAGE));
      body_ptr->dir_entry_cache_dirty[0] = TRUE;
     }
    else
     {
      /* Need to flush first */
      ret_val = meta_cache_flush_dir_cache(body_ptr,1);
      body_ptr->dir_entry_cache_dirty[1] = body_ptr->dir_entry_cache_dirty[0];
      memcpy(body_ptr->dir_entry_cache[1], body_ptr->dir_entry_cache[0], sizeof(DIR_ENTRY_PAGE));
      memcpy((body_ptr->dir_entry_cache[0]), temppage,sizeof(DIR_ENTRY_PAGE));
      body_ptr->dir_entry_cache_dirty[0] = TRUE;
     }
   }
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

  super_block_mark_dirty((body_ptr->this_stat).st_ino);

  return 0;
 }


int flush_single_meta_cache_entry(META_CACHE_ENTRY_STRUCT *body_ptr)
 {
  SUPER_BLOCK_ENTRY tempentry;
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

  /*Update stat info in super inode no matter what so that meta file get pushed to cloud*/
  /*TODO: May need to simply this so that only dirty status in super inode is updated */
  super_block_update_stat(body_ptr->inode_num, &(body_ptr->this_stat));

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
          flock(fileno((current_ptr->cache_entry_body).fptr),LOCK_UN);
          fclose((current_ptr->cache_entry_body).fptr);
          (current_ptr->cache_entry_body).meta_opened = FALSE;
         }

        free_single_meta_cache_entry(current_ptr);
        old_ptr = current_ptr;
        current_ptr = current_ptr->next;
        sem_post(&((old_ptr->cache_entry_body).access_sem));
        free(old_ptr);
        sem_wait(&num_entry_sem);
        current_meta_mem_cache_entries--;
        sem_post(&num_entry_sem);
       }
      meta_mem_cache[count].meta_cache_entries = NULL;
      meta_mem_cache[count].num_entries = 0;
      meta_mem_cache[count].last_entry = NULL;
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

  return 0;
 }

int hash_inode_to_meta_cache(ino_t this_inode)
 {
  return ((int) this_inode % NUM_META_MEM_CACHE_HEADERS);
 }

int meta_cache_update_file_data(ino_t this_inode, struct stat *inode_stat, FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page, long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
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
    if (body_ptr->meta_opened == FALSE)
     {
      ret_val = meta_cache_open_file(body_ptr);
      if (ret_val < 0)
       goto file_exception;
     }

    fseek(body_ptr->fptr,page_pos,SEEK_SET);
    fwrite(block_page,sizeof(BLOCK_ENTRY_PAGE),1,body_ptr->fptr);

    super_block_mark_dirty((body_ptr->this_stat).st_ino);
   }

  gettimeofday(&(body_ptr->last_access_time),NULL);

  if (body_ptr->something_dirty == FALSE)
   body_ptr->something_dirty = TRUE;

  if (META_CACHE_FLUSH_NOW == TRUE)
   ret_val = flush_single_meta_cache_entry(body_ptr);

  return 0;

/* Exception handling from here */
file_exception:
  return -1;

 }

int meta_cache_lookup_file_data(ino_t this_inode, struct stat *inode_stat, FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page, long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
 {
  int index;
  char thismetapath[METAPATHLEN];
  SUPER_BLOCK_ENTRY tempentry;
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
    if (body_ptr->meta_opened == FALSE)
     {
      ret_val = meta_cache_open_file(body_ptr);
      if (ret_val < 0)
       goto file_exception;
     }

    fseek(body_ptr->fptr,page_pos,SEEK_SET);
    fread(block_page,sizeof(BLOCK_ENTRY_PAGE),1,body_ptr->fptr);

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
  SUPER_BLOCK_ENTRY tempentry;
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
            body_ptr->dir_entry_cache[1] = malloc(sizeof(DIR_ENTRY_PAGE));
            body_ptr->dir_entry_cache_dirty[1] = body_ptr->dir_entry_cache_dirty[0];
            memcpy(body_ptr->dir_entry_cache[1], body_ptr->dir_entry_cache[0], sizeof(DIR_ENTRY_PAGE));

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
            if (body_ptr->dir_entry_cache_dirty[1] == TRUE)
             {
              fseek(body_ptr->fptr,(body_ptr->dir_entry_cache[1])->this_page_pos,SEEK_SET);
              fwrite(body_ptr->dir_entry_cache[1],sizeof(DIR_ENTRY_PAGE),1,body_ptr->fptr);
              super_block_mark_dirty((body_ptr->this_stat).st_ino);
             }

            memcpy(body_ptr->dir_entry_cache[1], body_ptr->dir_entry_cache[0], sizeof(DIR_ENTRY_PAGE));
            body_ptr->dir_entry_cache_dirty[1] = body_ptr->dir_entry_cache_dirty[0];

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

  if (META_CACHE_FLUSH_NOW == TRUE)
   ret_val = flush_single_meta_cache_entry(body_ptr);

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

  ret_val = search_dir_entry_btree(childname, &rootpage, fileno(body_ptr->fptr), &tmp_index, &tmp_resultpage);

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
  char found_entry,meta_opened;
  int can_use_index;

  index = hash_inode_to_meta_cache(this_inode);
/*First lock corresponding header*/
  sem_wait(&(meta_mem_cache[index].header_sem));

  current_ptr = meta_mem_cache[index].meta_cache_entries;
  prev_ptr = NULL;
  found_entry = FALSE;
  while(current_ptr!=NULL)
   {
    if (current_ptr->inode_num == this_inode) /* A hit */
     {
      found_entry = TRUE;
      break;
     }
    prev_ptr = current_ptr;
    current_ptr = current_ptr->next;
   }

  if (found_entry == FALSE) /*If did not find cache entry*/
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


  current_ptr->inode_num = 0;

  sem_post(&((current_ptr->cache_entry_body).access_sem));

  memset(body_ptr, 0, sizeof(META_CACHE_ENTRY_STRUCT));

  if (current_ptr->next != NULL)
   current_ptr->next->prev = prev_ptr;

  if (meta_mem_cache[index].last_entry == current_ptr)
   meta_mem_cache[index].last_entry = prev_ptr;

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
  
  sem_wait(&num_entry_sem);
  current_meta_mem_cache_entries--;
  sem_post(&num_entry_sem);

  sem_post(&(meta_mem_cache[index].header_sem));

  return 0;
 }

int expire_meta_mem_cache_entry()
 {
  /* Returns 0 if successfully expired one entry, -1 if not*/
  /* How to expire:
     Starting from some random header index, check from last_entry to find out if can
     lock some entry. If so, expire it if last_access_time > 0.5 sec. 
     Move to the next index if cannot expire anything, wrap back to 0 if overflow. */

  int start_index, current_index, ret_val;
  struct timeval current_time, *access_time_ptr;
  char expired;
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr;
  META_CACHE_ENTRY_STRUCT *body_ptr;
  double float_current_time, float_access_time;

  gettimeofday(&current_time, NULL);
  srandom((unsigned int)(current_time.tv_usec));
  start_index = (random() % NUM_META_MEM_CACHE_HEADERS);

  current_index = start_index;

  expired = FALSE;
  while (expired == FALSE)
   {
    sem_wait(&(meta_mem_cache[current_index].header_sem));
    current_ptr = meta_mem_cache[current_index].last_entry;
    while(current_ptr!=NULL)
     {
      ret_val = sem_trywait(&(current_ptr->cache_entry_body).access_sem);
      if (ret_val == 0)
       {
        gettimeofday(&current_time, NULL);
        float_current_time= (current_time.tv_sec * 1.0) + (current_time.tv_usec * 0.000001);
        access_time_ptr = &((current_ptr->cache_entry_body).last_access_time);
        float_access_time = (access_time_ptr->tv_sec * 1.0) + (access_time_ptr->tv_usec * 0.000001);
        if (float_current_time - float_access_time > 0.5)
         {
          /* Expire the entry */
          flush_single_meta_cache_entry(&(current_ptr->cache_entry_body));
          free_single_meta_cache_entry(current_ptr);
          if (current_ptr->next != NULL)
           current_ptr->next->prev = current_ptr->prev;
          if (current_ptr->prev != NULL)
           current_ptr->prev->next = current_ptr->next;
          if (meta_mem_cache[current_index].last_entry == current_ptr);
           meta_mem_cache[current_index].last_entry = current_ptr->prev;
          if (meta_mem_cache[current_index].meta_cache_entries == current_ptr);
           meta_mem_cache[current_index].meta_cache_entries = current_ptr->next;
          meta_mem_cache[current_index].num_entries--;
          sem_post(&(current_ptr->cache_entry_body).access_sem);
          free(current_ptr);
          sem_wait(&num_entry_sem);
          current_meta_mem_cache_entries--;
          sem_post(&num_entry_sem);
          sem_post(&(meta_mem_cache[current_index].header_sem));
          return 0;
         }
        else
         {
  /* If find that current_time < last_access_time, fix last_access_time to be current_time */
          if (float_current_time < float_access_time)
           gettimeofday(access_time_ptr, NULL);
         }
        sem_post(&(current_ptr->cache_entry_body).access_sem);
       }
      current_ptr = current_ptr->prev;
     }

    sem_post(&(meta_mem_cache[current_index].header_sem));

    current_index = ((current_index + 1) % NUM_META_MEM_CACHE_HEADERS);
    if (current_index == start_index)
     break;
   }

  return -1;
 }

META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
 {
  int ret_val;
  int index;
  META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr;
  META_CACHE_ENTRY_STRUCT *body_ptr;
  char need_new, expire_done;
  int can_use_index;
  int count;
  SUPER_BLOCK_ENTRY tempentry;
  META_CACHE_ENTRY_STRUCT *result_ptr, *prev_ptr;
  struct timespec time_to_sleep;

  time_to_sleep.tv_sec = 0;
  time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

  index = hash_inode_to_meta_cache(this_inode);
/*First lock corresponding header*/
  sem_wait(&(meta_mem_cache[index].header_sem));

  need_new = TRUE;

  while (need_new == TRUE)
   {
    current_ptr = meta_mem_cache[index].meta_cache_entries;
    while(current_ptr!=NULL)
     {
      if (current_ptr->inode_num == this_inode) /* A hit */
       {
        need_new = FALSE;
        break;
       }
      current_ptr = current_ptr->next;
     }
    if (need_new == FALSE)
     break;

    /*Probe whether entries full and expire some entry if full */
    sem_wait(&num_entry_sem);
    if (current_meta_mem_cache_entries > MAX_META_MEM_CACHE_ENTRIES)
     {
      sem_post(&num_entry_sem);

      /* Will need to release current header_sem, then reaquire after */
      sem_post(&(meta_mem_cache[index].header_sem));
      expire_done = FALSE;
      while(expire_done == FALSE)
       {
        ret_val = expire_meta_mem_cache_entry(index);
        if (ret_val < 0)
         {
          /* Sleep if cannot find one, then retry */
          nanosleep(&time_to_sleep,NULL);
         }
        else
         expire_done = TRUE;
       }
      sem_wait(&(meta_mem_cache[index].header_sem));
      continue;  /* Try again. Some other thread may have add in this entry */
     }
    else
     sem_post(&num_entry_sem);

    current_ptr = malloc(sizeof(META_CACHE_LOOKUP_ENTRY_STRUCT));
    if (current_ptr==NULL)
     return NULL;
/* TODO: Should return errno as well if needed */
/* return -EACCES; */

    memset(current_ptr,0,sizeof(META_CACHE_LOOKUP_ENTRY_STRUCT));
  
    current_ptr->next = meta_mem_cache[index].meta_cache_entries;
    if (meta_mem_cache[index].meta_cache_entries!=NULL)
     meta_mem_cache[index].meta_cache_entries->prev = current_ptr;
    meta_mem_cache[index].meta_cache_entries = current_ptr;
    if (meta_mem_cache[index].last_entry == NULL)
     meta_mem_cache[index].last_entry = current_ptr;
    current_ptr->inode_num = this_inode;
    sem_init(&((current_ptr->cache_entry_body).access_sem),0,1);
    meta_mem_cache[index].num_entries++;
    sem_wait(&num_entry_sem);
    current_meta_mem_cache_entries++;
    sem_post(&num_entry_sem);

    ret_val =super_block_read(this_inode, &tempentry);
    (current_ptr->cache_entry_body).inode_num = this_inode;
    (current_ptr->cache_entry_body).meta_opened = FALSE;
    memcpy(&((current_ptr->cache_entry_body).this_stat),&(tempentry.inode_stat),sizeof(struct stat));
    need_new = FALSE;
    break;
   }
/*Lock body*/
/*TODO: May need to add checkpoint here so that long sem wait will free all locks*/
  result_ptr = &(current_ptr->cache_entry_body);

  sem_wait(&((current_ptr->cache_entry_body).access_sem));
  sem_post(&(meta_mem_cache[index].header_sem));

/* Lock meta file if opened */
  if ((current_ptr->cache_entry_body).meta_opened == TRUE)
   flock(fileno((current_ptr->cache_entry_body).fptr),LOCK_EX);

  return result_ptr;
 }

int meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
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

  /* Unlock meta file if opened */
  if (target_ptr->meta_opened == TRUE)
   flock(fileno(target_ptr->fptr),LOCK_UN);

  sem_post(&(target_ptr->access_sem));

  return 0;
 }

int meta_cache_close_file(META_CACHE_ENTRY_STRUCT *target_ptr)
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
    flock(fileno(target_ptr->fptr),LOCK_UN);
    fclose(target_ptr->fptr);
    target_ptr->meta_opened = FALSE;
   }

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

