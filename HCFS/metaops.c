#include <sys/file.h>
#include "fuseop.h"
/*TODO: Will need to check if need to explicitly change st_atime, st_mtime*/
/*TODO: Consider whether to put changes to super_inode is_dirty here */

int dir_add_entry(ino_t parent_inode, ino_t child_inode, char *childname, mode_t child_mode)
 {
  FILE *parent_meta;
  char parent_meta_name[400];
  DIR_META_TYPE parent_meta_head;
  DIR_ENTRY_PAGE temppage;
  int ret_items;
  long nextfilepos,oldfilepos;

  fetch_meta_path(parent_meta_name,parent_inode);

  parent_meta =fopen(parent_meta_name,"r+");
  if (parent_meta==NULL)
   return -1;
  setbuf(parent_meta,NULL);
  flock(fileno(parent_meta),LOCK_EX);
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
    if (temppage.num_entries < MAX_DIR_ENTRIES_PER_PAGE)
     {   /*This page has empty dir entry. Fill it.*/
      temppage.dir_entries[temppage.num_entries].d_ino = child_inode;
      strcpy(temppage.dir_entries[temppage.num_entries].d_name,childname);
      temppage.num_entries++;
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
      /*If the new entry is a subdir, increase the hard link of the parent*/

      if (child_mode & S_IFDIR)
       {
        parent_meta_head.thisstat.st_nlink++;
        fseek(parent_meta,0,SEEK_SET);
        ret_items = fwrite(&parent_meta_head,sizeof(DIR_META_TYPE),1,parent_meta);
        if (ret_items <1)
         {
          flock(fileno(parent_meta),LOCK_UN);
          fclose(parent_meta);
          return -1;
         }
       }
 
      flock(fileno(parent_meta),LOCK_UN);
      fclose(parent_meta);

      return 0;
     }
    if (temppage.next_page == 0)
     break;
    nextfilepos = temppage.next_page;
   }

  /*Will need to allocate a new page*/
  oldfilepos = nextfilepos;

  fseek(parent_meta,0,SEEK_END);
  nextfilepos=ftell(parent_meta);

  if (oldfilepos!=0)
   {
    temppage.next_page = nextfilepos;
    fseek(parent_meta,oldfilepos,SEEK_SET);
    ret_items = fwrite(&temppage,sizeof(DIR_ENTRY_PAGE),1,parent_meta);
    if (ret_items <1)
     {
      flock(fileno(parent_meta),LOCK_UN);
      fclose(parent_meta);
      return -1;
     }
    if (child_mode & S_IFDIR)
     {
      parent_meta_head.thisstat.st_nlink++;
      fseek(parent_meta,0,SEEK_SET);
      ret_items = fwrite(&parent_meta_head,sizeof(DIR_META_TYPE),1,parent_meta);
      if (ret_items <1)
       {
        flock(fileno(parent_meta),LOCK_UN);
        fclose(parent_meta);
        return -1;
       }
     }
   }
  else
   {
    if (child_mode & S_IFREG)
     {
      parent_meta_head.next_file_page=nextfilepos;
     }
    else
     {
      if (child_mode & S_IFDIR)
       {
        parent_meta_head.next_subdir_page=nextfilepos;
        parent_meta_head.thisstat.st_nlink++;
       }
     }
    fseek(parent_meta,0,SEEK_SET);
    ret_items = fwrite(&parent_meta_head,sizeof(DIR_META_TYPE),1,parent_meta);
    if (ret_items <1)
     {
      flock(fileno(parent_meta),LOCK_UN);
      fclose(parent_meta);
      return -1;
     }
   }
  fseek(parent_meta,nextfilepos,SEEK_SET);
  memset(&temppage,0,sizeof(DIR_ENTRY_PAGE));
  temppage.num_entries=1;
  temppage.dir_entries[0].d_ino = child_inode;
  strcpy(temppage.dir_entries[0].d_name,childname);
  ret_items = fwrite(&temppage,sizeof(DIR_ENTRY_PAGE),1,parent_meta);
  if (ret_items <1)
   {
    flock(fileno(parent_meta),LOCK_UN);
    fclose(parent_meta);
    return -1;
   }

  flock(fileno(parent_meta),LOCK_UN);
  fclose(parent_meta);
  return 0;
 }


int dir_remove_entry(ino_t parent_inode, ino_t child_inode, char *childname, mode_t child_mode)
 {
  FILE *parent_meta;
  char parent_meta_name[400];
  DIR_META_TYPE parent_meta_head;
  DIR_ENTRY_PAGE temppage;
  int ret_items;
  long nextfilepos,oldfilepos;
  int count;

  fetch_meta_path(parent_meta_name,parent_inode);

  parent_meta =fopen(parent_meta_name,"r+");
  if (parent_meta==NULL)
   return -1;
  setbuf(parent_meta,NULL);
  flock(fileno(parent_meta),LOCK_EX);
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
          parent_meta_head.thisstat.st_nlink--;
          fseek(parent_meta,0,SEEK_SET);
          ret_items = fwrite(&parent_meta_head,sizeof(DIR_META_TYPE),1,parent_meta);
          if (ret_items <1)
           {
            flock(fileno(parent_meta),LOCK_UN);
            fclose(parent_meta);
            return -1;
           }
         }
 
        flock(fileno(parent_meta),LOCK_UN);
        fclose(parent_meta);

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
  char parent_meta_name[400];
  DIR_META_TYPE parent_meta_head;
  DIR_ENTRY_PAGE temppage;
  int ret_items;
  long nextfilepos,oldfilepos;
  int count;

  fetch_meta_path(parent_meta_name,parent_inode);

  parent_meta =fopen(parent_meta_name,"r+");
  if (parent_meta==NULL)
   return -1;
  setbuf(parent_meta,NULL);
  flock(fileno(parent_meta),LOCK_EX);
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
  char self_meta_name[400];
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

      return 0;
     }
   }
  flock(fileno(self_meta),LOCK_UN);
  fclose(self_meta);
  return -1;
 }
