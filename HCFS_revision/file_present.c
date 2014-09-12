#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "global.h"
#include "file_present.h"
#include "super_inode.h"
#include "fuseop.h"
#include "params.h"

#include "meta_mem_cache.h"

int meta_forget_inode(ino_t self_inode)
 {
  char thismetapath[METAPATHLEN];

  fetch_meta_path(thismetapath,self_inode);

  if (access(thismetapath,F_OK)==0)
   {
    unlink(thismetapath);
   }

 /*TODO: Need to remove entry from super inode if needed */

  return 0;
 }

int fetch_inode_stat(ino_t this_inode, struct stat *inode_stat)
 {
  struct stat returned_stat;
  int ret_code;

  /*First will try to lookup meta cache*/
  if (this_inode > 0)
   {
    /*Only fetch inode stat, so does not matter if inode is reg file or dir here*/
    ret_code = meta_cache_lookup_file_data(this_inode, &returned_stat, NULL, NULL, 0);

    if (ret_code == 0)
     {
      memcpy(inode_stat, &returned_stat,sizeof(struct stat));
      return 0;
     }

    if (ret_code < 0)
     {
      /* TODO: For performance reason, may not want to try meta files. System could only test if some file really does not exist */
      /* TODO: This option should only be turned on if system is in some sort of recovery mode. In this case, should even try backend meta backups */

      //ret_code = inode_stat_from_meta(this_inode, inode_stat);
      /* TODO: Perhaps missing from super inode (inode caching?). Fill it in? */
     }

    if (ret_code < 0)
     return -ENOENT;
   }
  else
   return -ENOENT;

/*TODO: What to do if cannot create new meta cache entry? */

  #if DEBUG >= 5
  printf("fetch_inode_stat %lld\n",inode_stat->st_ino);
  #endif  /* DEBUG */


  return 0;
 }

//int inode_stat_from_meta(ino_t this_inode, struct stat *inode_stat)

int mknod_update_meta(ino_t self_inode, ino_t parent_inode, char *selfname, struct stat *this_stat)
 {
  int ret_val;
  FILE_META_TYPE this_meta;

  memset(&this_meta,0,sizeof(FILE_META_TYPE));

  ret_val = meta_cache_update_file_data(self_inode,this_stat, &this_meta,NULL, 0);

  if (ret_val < 0)
   {
    return -EACCES;
   }

  ret_val = dir_add_entry(parent_inode, self_inode, selfname,this_stat->st_mode);
  if (ret_val < 0)
   {
    return -EACCES;
   }

  return 0;
 }


int mkdir_update_meta(ino_t self_inode, ino_t parent_inode, char *selfname, struct stat *this_stat)
 {
  char thismetapath[METAPATHLEN];
  DIR_META_TYPE this_meta;
  DIR_ENTRY_PAGE temppage;
  FILE *metafptr;
  int ret_val;

  memset(&this_meta,0,sizeof(DIR_META_TYPE));
  memset(&temppage,0,sizeof(DIR_ENTRY_PAGE));

  this_meta.root_entry_page = sizeof(struct stat)+sizeof(DIR_META_TYPE);
  init_dir_page(&temppage, self_inode, parent_inode, this_meta.root_entry_page);

  ret_val = meta_cache_update_dir_data(self_inode,this_stat, &this_meta,&temppage, this_meta.root_entry_page);

  if (ret_val < 0)
   {
    return -EACCES;
   }

  ret_val = dir_add_entry(parent_inode, self_inode, selfname, this_stat->st_mode);
  if (ret_val < 0)
   {
    return -EACCES;
   }

  return 0;
 }

int unlink_update_meta(ino_t parent_inode, ino_t this_inode,char *selfname)
 {
  int ret_val;

  ret_val = dir_remove_entry(parent_inode,this_inode,selfname,S_IFREG);
  if (ret_val < 0)
   return -EACCES;

  ret_val = decrease_nlink_inode_file(this_inode);

  return ret_val;
 }

int rmdir_update_meta(ino_t parent_inode, ino_t this_inode, char *selfname)
 {
  DIR_META_TYPE tempmeta;
  char thismetapath[METAPATHLEN];
  char todelete_metapath[METAPATHLEN];
  int ret_val;
  FILE *todeletefptr, *metafptr;
  char filebuf[5000];
  size_t read_size;

  ret_val = meta_cache_lookup_dir_data(this_inode, NULL, &tempmeta, NULL, 0);

  printf("TOTAL CHILDREN is now %ld\n",tempmeta.total_children);

  if (tempmeta.total_children > 0)
   return -ENOTEMPTY;

  ret_val = dir_remove_entry(parent_inode,this_inode,selfname,S_IFDIR);
  if (ret_val < 0)
   return -EACCES;

  /*Need to delete the inode*/
  super_inode_to_delete(this_inode);
  fetch_todelete_path(todelete_metapath,this_inode);
  /*Try a rename first*/
  ret_val = rename(thismetapath,todelete_metapath);
  if (ret_val < 0)
   {
    /*If not successful, copy the meta*/
    unlink(todelete_metapath);
    todeletefptr = fopen(todelete_metapath,"w");
    fetch_meta_path(thismetapath,this_inode);
    metafptr = fopen(thismetapath,"r");
    setbuf(metafptr,NULL); 
    flock(fileno(metafptr),LOCK_EX);
    setbuf(todeletefptr,NULL);
    fseek(metafptr,0,SEEK_SET);

    while(!feof(metafptr))
     {
      read_size = fread(filebuf,1,4096,metafptr);
      if (read_size > 0)
       {
        fwrite(filebuf,1,read_size,todeletefptr);
       }
      else
       break;
     }
    fclose(todeletefptr);

    unlink(thismetapath);
    flock(fileno(metafptr),LOCK_UN);
    fclose(metafptr);
    ret_val = meta_cache_remove(this_inode);
   }

  return ret_val;
 }
