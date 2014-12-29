#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <attr/xattr.h>
#include <semaphore.h>
#include <sys/mman.h>

#include "fuseop.h"
#include "params.h"
#include "global.h"
#include "meta_mem_cache.h"
#include "filetables.h"

int init_system_fh_table()
 {
  long long count;

  memset(&system_fh_table,0,sizeof(FH_TABLE_TYPE));

  system_fh_table.entry_table_flags = malloc(sizeof(char) * MAX_OPEN_FILE_ENTRIES);
  if (system_fh_table.entry_table_flags == NULL)
   return -1;
  memset(system_fh_table.entry_table_flags,0,sizeof(char) * MAX_OPEN_FILE_ENTRIES);

  system_fh_table.entry_table = malloc(sizeof(FH_ENTRY) * MAX_OPEN_FILE_ENTRIES);
  if (system_fh_table.entry_table == NULL)
   return -1;

  memset(system_fh_table.entry_table,0,sizeof(FH_ENTRY) * MAX_OPEN_FILE_ENTRIES);

  system_fh_table.last_available_index = 0;

  sem_init(&(system_fh_table.fh_table_sem),0,1);
  return 0;
 }

long long open_fh(ino_t thisinode)
 {
  long long index;
  char thismetapath[METAPATHLEN];

  sem_wait(&(system_fh_table.fh_table_sem));

  if (system_fh_table.num_opened_files >= MAX_OPEN_FILE_ENTRIES)
   {
    sem_post(&(system_fh_table.fh_table_sem));
    return -1;   /*Not able to allocate any more fh entry as table is full.*/
   }

  index = system_fh_table.last_available_index % MAX_OPEN_FILE_ENTRIES;
  while(system_fh_table.entry_table_flags[index]==TRUE)
   {
    index++;
    index = index % MAX_OPEN_FILE_ENTRIES;
   }

  system_fh_table.entry_table_flags[index]=TRUE;
  system_fh_table.entry_table[index].meta_cache_ptr = NULL;
  system_fh_table.entry_table[index].meta_cache_locked = FALSE;
  system_fh_table.entry_table[index].thisinode = thisinode;

  system_fh_table.entry_table[index].blockfptr = NULL;
  system_fh_table.entry_table[index].opened_block = -1;
  system_fh_table.entry_table[index].cached_page_index = -1;
  system_fh_table.entry_table[index].cached_filepos = -1;
  sem_init(&(system_fh_table.entry_table[index].block_sem),0,1);

  sem_post(&(system_fh_table.fh_table_sem));
  return index;
 }

int close_fh(long long index)
 {
  sem_wait(&(system_fh_table.fh_table_sem));

  if (system_fh_table.entry_table_flags[index]==TRUE)
   {
    if (system_fh_table.entry_table[index].meta_cache_locked == FALSE)
     {
      system_fh_table.entry_table[index].meta_cache_ptr = meta_cache_lock_entry(system_fh_table.entry_table[index].thisinode);
      system_fh_table.entry_table[index].meta_cache_locked = TRUE;
     }
    meta_cache_close_file(system_fh_table.entry_table[index].meta_cache_ptr);

    system_fh_table.entry_table[index].meta_cache_locked = FALSE;
    meta_cache_unlock_entry(system_fh_table.entry_table[index].meta_cache_ptr);

    system_fh_table.entry_table_flags[index]=FALSE;
    system_fh_table.entry_table[index].thisinode = 0;

    if ((system_fh_table.entry_table[index].blockfptr!=NULL) && (system_fh_table.entry_table[index].opened_block>=0))
     fclose(system_fh_table.entry_table[index].blockfptr);

    system_fh_table.entry_table[index].meta_cache_ptr = NULL;
    system_fh_table.entry_table[index].blockfptr = NULL;
    system_fh_table.entry_table[index].opened_block = -1;
    sem_destroy(&(system_fh_table.entry_table[index].block_sem));
    system_fh_table.last_available_index = index;
   }

  sem_post(&(system_fh_table.fh_table_sem));
  return 0;
 }

int seek_page(FH_ENTRY *fh_ptr,long long target_page)
 {
  long long current_page;
  off_t nextfilepos, prevfilepos, currentfilepos;
  BLOCK_ENTRY_PAGE temppage;
  META_CACHE_ENTRY_STRUCT *body_ptr;
  int sem_val;
  FILE_META_TYPE temp_meta;

  /* First check if meta cache is locked */

  body_ptr = fh_ptr->meta_cache_ptr;

  sem_getvalue(&(body_ptr->access_sem), &sem_val);
  if (sem_val > 0)
   {
    /*Not locked, return -1*/
    return -1;
   }

  meta_cache_lookup_file_data(fh_ptr->thisinode, NULL, &temp_meta, NULL, 0, body_ptr);

  nextfilepos=temp_meta.next_block_page;
  current_page = 0;
  prevfilepos = 0;

  meta_cache_open_file(body_ptr);

  /*TODO: put error handling for the read/write ops here*/
  while(current_page <= target_page)
   {
    if (nextfilepos == 0) /*Need to append a new block entry page */
     {
      if (prevfilepos == 0) /* If not even the first page is generated */
       {
        fseek(body_ptr->fptr, 0, SEEK_END);
        prevfilepos = ftell(body_ptr->fptr);
        temp_meta.next_block_page = prevfilepos;
        memset(&temppage,0,sizeof(BLOCK_ENTRY_PAGE));
        meta_cache_update_file_data(fh_ptr->thisinode, NULL, &temp_meta, &temppage, prevfilepos, body_ptr);
       }
      else
       {
        fseek(body_ptr->fptr, 0, SEEK_END);
        currentfilepos = ftell(body_ptr->fptr);
        meta_cache_lookup_file_data(fh_ptr->thisinode, NULL, NULL, &temppage, prevfilepos, body_ptr);
        temppage.next_page = currentfilepos;
        meta_cache_update_file_data(fh_ptr->thisinode, NULL, NULL, &temppage, prevfilepos, body_ptr);

        memset(&temppage,0,sizeof(BLOCK_ENTRY_PAGE));
        meta_cache_update_file_data(fh_ptr->thisinode, NULL, NULL, &temppage, currentfilepos, body_ptr);

        prevfilepos = currentfilepos;
       }
     }
    else
     {
      meta_cache_lookup_file_data(fh_ptr->thisinode, NULL, NULL, &temppage, nextfilepos, body_ptr);

      prevfilepos = nextfilepos;
      nextfilepos = temppage.next_page;
     }
    if (current_page == target_page)
     break;
    else
     current_page++;
   }
  fh_ptr->cached_page_index = target_page;
  fh_ptr->cached_filepos = prevfilepos;

  return 0;
 }

long long advance_block(META_CACHE_ENTRY_STRUCT *body_ptr, off_t thisfilepos,long long *entry_index)
 {
  long long temp_index;
  off_t nextfilepos;
  BLOCK_ENTRY_PAGE temppage;
  int ret_val;
  /*First handle the case that nothing needs to be changed, just add entry_index*/

  temp_index = *entry_index;
  if ((temp_index+1) < MAX_BLOCK_ENTRIES_PER_PAGE)
   {
    temp_index++;
    *entry_index = temp_index;
    return thisfilepos;
   }

  /*We need to change to another page*/

  ret_val = meta_cache_open_file(body_ptr);

  fseek(body_ptr->fptr,thisfilepos,SEEK_SET);
  fread(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,body_ptr->fptr);
  nextfilepos = temppage.next_page;

  if (nextfilepos == 0)   /*Need to allocate a new page*/
   {
    fseek(body_ptr->fptr,0,SEEK_END);
    nextfilepos = ftell(body_ptr->fptr);
    temppage.next_page = nextfilepos;
    fseek(body_ptr->fptr, thisfilepos,SEEK_SET);
    fwrite(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,body_ptr->fptr);
    fseek(body_ptr->fptr,nextfilepos,SEEK_SET);
    memset(&temppage,0,sizeof(BLOCK_ENTRY_PAGE));
    fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,body_ptr->fptr);
   }

  *entry_index = 0;
  return nextfilepos;
 }
