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
    fsee(parent_meta,oldfilepos,SEEK_SET);
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

