#include <sys/file.h>
#include "global.h"
#include "utils.h"
#include "fuseop.h"
#include "file_present.h"
#include "params.h"
/*TODO: Will need to check if need to explicitly change st_atime, st_mtime*/
/* TODO: Need to consider directory access rights here or in fuseop */

int dir_add_entry(ino_t parent_inode, ino_t child_inode, char *childname, mode_t child_mode)
 {
  FILE *parent_meta;
  struct stat parent_meta_stat;
  DIR_META_TYPE parent_meta_head;
  DIR_ENTRY_PAGE temppage;
  int ret_items;
  int ret_val;
  long page_pos;

  ret_val = meta_cache_lookup_dir_data(parent_inode, &parent_meta_stat,&parent_meta_head,NULL,0);

  ret_val = meta_cache_seek_empty_dir_entry(parent_inode,&temppage,&page_pos);

  entry_index = temppage.num_entries;
  temppage.num_entries++;

  temppage.dir_entries[temppage.num_entries].d_ino = child_inode;
  strcpy(temppage.dir_entries[temppage.num_entries].d_name,childname);

  /*If the new entry is a subdir, increase the hard link of the parent*/

  if (child_mode & S_IFDIR)
   {
    parent_meta_stat.st_nlink++;
   }

  parent_meta_head.total_children++;
  printf("TOTAL CHILDREN is now %ld\n",parent_meta_head.total_children);

  ret_val = meta_cache_update_dir_data(parent_inode, &parent_meta_stat,&parent_meta_head,&temppage,page_pos);

  return ret_val;
 }


int dir_remove_entry(ino_t parent_inode, ino_t child_inode, char *childname, mode_t child_mode)
 {
  FILE *parent_meta;
  char parent_meta_name[METAPATHLEN];
  struct stat parent_meta_stat;
  DIR_META_TYPE parent_meta_head;
  DIR_ENTRY_PAGE temppage;
  int ret_items;
  off_t nextfilepos,oldfilepos;
  int count;

  fetch_meta_path(parent_meta_name,parent_inode);

  parent_meta =fopen(parent_meta_name,"r+");
  if (parent_meta==NULL)
   return -1;
  setbuf(parent_meta,NULL);
  flock(fileno(parent_meta),LOCK_EX);

  ret_items = fread(&parent_meta_stat,sizeof(struct stat),1,parent_meta);
  if (ret_items <1)
   {
    flock(fileno(parent_meta),LOCK_UN);
    fclose(parent_meta);
    return -1;
   }

  ret_items = fread(&parent_meta_head,sizeof(DIR_META_TYPE),1,parent_meta);
  if (ret_items <1)
   {
    flock(fileno(parent_meta),LOCK_UN);
    fclose(parent_meta);
    return -1;
   }

  if (child_mode & S_IFREG)
   {
    nextfilepos=parent_meta_head.next_file_page;
   }
  else
   {
    if (child_mode & S_IFDIR)
     nextfilepos=parent_meta_head.next_subdir_page;
   }
  memset(&temppage,0,sizeof(DIR_ENTRY_PAGE));
  while(nextfilepos!=0)
   {
    fseek(parent_meta,nextfilepos,SEEK_SET);
    if (ftell(parent_meta)!=nextfilepos)
     {
      flock(fileno(parent_meta),LOCK_UN);
      fclose(parent_meta);
      return -1;
     }
    ret_items = fread(&temppage,sizeof(DIR_ENTRY_PAGE),1,parent_meta);
    if (ret_items <1)
     {
      flock(fileno(parent_meta),LOCK_UN);
      fclose(parent_meta);
      return -1;
     }

    for(count=0;count<temppage.num_entries;count++)
     {
      if ((strcmp(temppage.dir_entries[count].d_name,childname)==0) &&
           (temppage.dir_entries[count].d_ino == child_inode))
       {  /*Found the entry. Delete it*/
        if (count!=(temppage.num_entries-1))
         memcpy(&(temppage.dir_entries[count]), &(temppage.dir_entries[temppage.num_entries-1]), sizeof(DIR_ENTRY));
        temppage.num_entries--;
        memset(&(temppage.dir_entries[temppage.num_entries]),0,sizeof(DIR_ENTRY));

        fseek(parent_meta,nextfilepos,SEEK_SET);
        if (ftell(parent_meta)!=nextfilepos)
         {
          flock(fileno(parent_meta),LOCK_UN);
          fclose(parent_meta);
          return -1;
         }
        ret_items = fwrite(&temppage,sizeof(DIR_ENTRY_PAGE),1,parent_meta);
        if (ret_items <1)
         {
          flock(fileno(parent_meta),LOCK_UN);
          fclose(parent_meta);
          return -1;
         }
      /*If the new entry is a subdir, decrease the hard link of the parent*/

        if (child_mode & S_IFDIR)
         {
          parent_meta_stat.st_nlink--;
          fseek(parent_meta,0,SEEK_SET);
          ret_items = fwrite(&parent_meta_stat,sizeof(struct stat),1,parent_meta);
          if (ret_items <1)
           {
            flock(fileno(parent_meta),LOCK_UN);
            fclose(parent_meta);
            return -1;
           }
         }
 
        parent_meta_head.total_children--;
        printf("TOTAL CHILDREN is now %ld\n",parent_meta_head.total_children);
        fseek(parent_meta,sizeof(struct stat),SEEK_SET);
        ret_items = fwrite(&parent_meta_head,sizeof(DIR_META_TYPE),1,parent_meta);
        if (ret_items <1)
         {
          flock(fileno(parent_meta),LOCK_UN);
          fclose(parent_meta);
          return -1;
         }

        flock(fileno(parent_meta),LOCK_UN);
        fclose(parent_meta);

        super_inode_mark_dirty(parent_inode);

        return 0;
       }
     }
    if (temppage.next_page == 0)
     break;
    nextfilepos = temppage.next_page;
   }

  flock(fileno(parent_meta),LOCK_UN);
  fclose(parent_meta);
  return -1;
 }

int dir_replace_name(ino_t parent_inode, ino_t child_inode, char *oldname, char *newname, mode_t child_mode)
 {
  FILE *parent_meta;
  char parent_meta_name[METAPATHLEN];
  DIR_META_TYPE parent_meta_head;
  DIR_ENTRY_PAGE temppage;
  int ret_items;
  off_t nextfilepos,oldfilepos;
  int count;

  fetch_meta_path(parent_meta_name,parent_inode);

  parent_meta =fopen(parent_meta_name,"r+");
  if (parent_meta==NULL)
   return -1;
  setbuf(parent_meta,NULL);
  flock(fileno(parent_meta),LOCK_EX);
  fseek(parent_meta,sizeof(struct stat),SEEK_SET);
  ret_items = fread(&parent_meta_head,sizeof(DIR_META_TYPE),1,parent_meta);
  if (ret_items <1)
   {
    flock(fileno(parent_meta),LOCK_UN);
    fclose(parent_meta);
    return -1;
   }

  if (child_mode & S_IFREG)
   {
    nextfilepos=parent_meta_head.next_file_page;
   }
  else
   {
    if (child_mode & S_IFDIR)
     nextfilepos=parent_meta_head.next_subdir_page;
   }
  memset(&temppage,0,sizeof(DIR_ENTRY_PAGE));
  while(nextfilepos!=0)
   {
    fseek(parent_meta,nextfilepos,SEEK_SET);
    if (ftell(parent_meta)!=nextfilepos)
     {
      flock(fileno(parent_meta),LOCK_UN);
      fclose(parent_meta);
      return -1;
     }
    ret_items = fread(&temppage,sizeof(DIR_ENTRY_PAGE),1,parent_meta);
    if (ret_items <1)
     {
      flock(fileno(parent_meta),LOCK_UN);
      fclose(parent_meta);
      return -1;
     }

    for(count=0;count<temppage.num_entries;count++)
     {
      if ((strcmp(temppage.dir_entries[count].d_name,oldname)==0) &&
           (temppage.dir_entries[count].d_ino == child_inode))
       {  /*Found the entry. Replace it*/
        strcpy(temppage.dir_entries[count].d_name,newname);

        fseek(parent_meta,nextfilepos,SEEK_SET);
        if (ftell(parent_meta)!=nextfilepos)
         {
          flock(fileno(parent_meta),LOCK_UN);
          fclose(parent_meta);
          return -1;
         }
        ret_items = fwrite(&temppage,sizeof(DIR_ENTRY_PAGE),1,parent_meta);
        if (ret_items <1)
         {
          flock(fileno(parent_meta),LOCK_UN);
          fclose(parent_meta);
          return -1;
         }
 
        flock(fileno(parent_meta),LOCK_UN);
        fclose(parent_meta);
        super_inode_mark_dirty(parent_inode);

        return 0;
       }
     }
    if (temppage.next_page == 0)
     break;
    nextfilepos = temppage.next_page;
   }

  flock(fileno(parent_meta),LOCK_UN);
  fclose(parent_meta);
  return -1;
 }

int change_parent_inode(ino_t self_inode, ino_t parent_inode1, ino_t parent_inode2)
 {
  FILE *self_meta;
  char self_meta_name[METAPATHLEN];
  DIR_META_TYPE self_meta_head;
  DIR_ENTRY_PAGE temppage;
  int ret_items;
  int count;

  fetch_meta_path(self_meta_name,self_inode);

  self_meta =fopen(self_meta_name,"r+");
  if (self_meta==NULL)
   return -1;
  setbuf(self_meta,NULL);
  flock(fileno(self_meta),LOCK_EX);
  fseek(self_meta,sizeof(struct stat),SEEK_SET);
  ret_items = fread(&self_meta_head,sizeof(DIR_META_TYPE),1,self_meta);
  if (ret_items <1)
   {
    flock(fileno(self_meta),LOCK_UN);
    fclose(self_meta);
    return -1;
   }

  memset(&temppage,0,sizeof(DIR_ENTRY_PAGE));
  fseek(self_meta,self_meta_head.next_subdir_page,SEEK_SET);
  if (ftell(self_meta)!=self_meta_head.next_subdir_page)
   {
    flock(fileno(self_meta),LOCK_UN);
    fclose(self_meta);
    return -1;
   }
  ret_items = fread(&temppage,sizeof(DIR_ENTRY_PAGE),1,self_meta);
  if (ret_items <1)
   {
    flock(fileno(self_meta),LOCK_UN);
    fclose(self_meta);
    return -1;
   }

  for(count=0;count<temppage.num_entries;count++)
   {
    if (strcmp(temppage.dir_entries[count].d_name,"..")==0)
     {
      if (temppage.dir_entries[count].d_ino!=parent_inode1)
       {
        flock(fileno(self_meta),LOCK_UN);
        fclose(self_meta);
        return -1;
       }
      temppage.dir_entries[count].d_ino = parent_inode2;
      fseek(self_meta,self_meta_head.next_subdir_page,SEEK_SET);
      if (ftell(self_meta)!=self_meta_head.next_subdir_page)
       {
        flock(fileno(self_meta),LOCK_UN);
        fclose(self_meta);
        return -1;
       }
      ret_items = fwrite(&temppage,sizeof(DIR_ENTRY_PAGE),1,self_meta);
      if (ret_items <1)
       {
        flock(fileno(self_meta),LOCK_UN);
        fclose(self_meta);
        return -1;
       }
 
      flock(fileno(self_meta),LOCK_UN);
      fclose(self_meta);
      super_inode_mark_dirty(self_inode);

      return 0;
     }
   }
  flock(fileno(self_meta),LOCK_UN);
  fclose(self_meta);
  return -1;
 }

int decrease_nlink_inode_file(ino_t this_inode)
 {
  char thismetapath[METAPATHLEN];
  char todelete_metapath[400];
  char thisblockpath[400];
  char filebuf[5000];
  struct stat this_inode_stat;
  FILE *metafptr;
  FILE *todeletefptr;
  int ret_val;
  long long count;
  long long total_blocks;
  off_t cache_block_size;
  size_t read_size;

  fetch_meta_path(thismetapath,this_inode);

  metafptr = fopen(thismetapath,"r+");
  if (metafptr == NULL)
   return -EACCES;
  setbuf(metafptr,NULL);
  flock(fileno(metafptr),LOCK_EX);

  ret_val = fread(&this_inode_stat,sizeof(struct stat),1,metafptr);
  if (ret_val < 1)
   {
    flock(fileno(metafptr),LOCK_UN);
    fclose(metafptr);
    return -EACCES;
   }

  if (this_inode_stat.st_nlink<=1)
   {
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
     }

    /*Need to delete blocks as well*/
    /*TODO: Perhaps can move the actual block deletion to the deletion loop as well*/
    if (this_inode_stat.st_size == 0)
     total_blocks = 0;
    else
     total_blocks = ((this_inode_stat.st_size-1) / MAX_BLOCK_SIZE) + 1;
    for(count = 0;count<total_blocks;count++)
     {
      fetch_block_path(thisblockpath,this_inode,count);
      if (!access(thisblockpath,F_OK))
       {
        cache_block_size = check_file_size(thisblockpath);
        unlink(thisblockpath);
        sem_wait(&(hcfs_system->access_sem));
        hcfs_system->systemdata.cache_size -= (long long) cache_block_size;
        hcfs_system->systemdata.cache_blocks -=1;
        sem_post(&(hcfs_system->access_sem));           
       }
     }
    sem_wait(&(hcfs_system->access_sem));
    hcfs_system->systemdata.system_size -= this_inode_stat.st_size;
    sync_hcfs_system_data(FALSE);
    sem_post(&(hcfs_system->access_sem));           

    flock(fileno(metafptr),LOCK_UN);
    fclose(metafptr);

   }
  else
   {
    this_inode_stat.st_nlink--; 
    fseek(metafptr,0,SEEK_SET);
    ret_val = fwrite(&this_inode_stat,sizeof(struct stat),1,metafptr);
    if (ret_val < 1)
     {
      flock(fileno(metafptr),LOCK_UN);
      fclose(metafptr);
      return -EACCES;
     }
    super_inode_update_stat(this_inode, &(this_inode_stat));
    flock(fileno(metafptr),LOCK_UN);

    super_inode_mark_dirty(this_inode);

    fclose(metafptr);
   }

  return 0;
 }    

