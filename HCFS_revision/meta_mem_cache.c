#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "params.h"
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
 

  if (entry_ptr->something_dirty == FALSE)
   return 0;

  body_ptr = &(entry_ptr->cache_entry_body);

  fetch_meta_path(thismetapath,entry_ptr->inode_num);

  fptr = fopen(thismetapath,"r+");
  if (fptr==NULL)
   return -ENOENT;
  setbuf(fptr,NULL);
  
  flock(fileno(fptr),LOCK_EX);

/*TODO: put what to flush back here */

  if (body_ptr->stat_dirty == True)
   {
    fseek(fptr,0,SEEK_SET);
    fwrite(&(body_ptr->this_stat),sizeof(struct stat),1,fptr);
   }

  if (S_ISREG(body_ptr->inode_mode) == True)
   {
    if (body_ptr->meta_dirty == True)
     {
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fwrite(&(body_ptr->file_meta),sizeof(FILE_META_TYPE),1,fptr);
     }
    if ((body_ptr->block_entry_cache_dirty[0] == True) && (body_ptr->block_entry_cache[0] != NULL))
     {
      fseek(fptr,body_ptr->block_entry_cache_pos[0],SEEK_SET);
      fwrite(body_ptr->block_entry_cache[0],sizeof(BLOCK_ENTRY_PAGE),1,fptr);
     }
    if ((body_ptr->block_entry_cache_dirty[1] == True) && (body_ptr->block_entry_cache[1] != NULL))
     {
      fseek(fptr,body_ptr->block_entry_cache_pos[1],SEEK_SET);
      fwrite(body_ptr->block_entry_cache[1],sizeof(BLOCK_ENTRY_PAGE),1,fptr);
     }
   }

  /*TODO: Add flush of xattr pages here */

  flock(fileno(fptr),LOCK_UN);
  fclose(fptr);
  if (body_ptr->stat_dirty == True)
   super_inode_update_stat(this_inode, &(body_ptr->this_stat));

