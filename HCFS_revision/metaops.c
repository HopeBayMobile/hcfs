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
  struct stat parent_meta_stat;
  DIR_META_TYPE parent_meta_head;
  DIR_ENTRY_PAGE temppage;
  int ret_items;
  int ret_val;
  long page_pos;
  int entry_index;

  ret_val = meta_cache_lookup_dir_data(parent_inode, &parent_meta_stat, &parent_meta_head, NULL,0,0);

  ret_val = meta_cache_seek_empty_dir_entry(parent_inode,&temppage,&page_pos,child_mode, &parent_meta_head);

  printf("debug dir_add_entry page_pos 1 %ld\n",page_pos);

  entry_index = temppage.num_entries;
  temppage.num_entries++;

  temppage.dir_entries[entry_index].d_ino = child_inode;
  strcpy(temppage.dir_entries[entry_index].d_name,childname);

  /*If the new entry is a subdir, increase the hard link of the parent*/

  if (child_mode & S_IFDIR)
   {
    parent_meta_stat.st_nlink++;
   }

  parent_meta_head.total_children++;
  printf("TOTAL CHILDREN is now %ld\n",parent_meta_head.total_children);

  ret_val = meta_cache_update_dir_data(parent_inode, &parent_meta_stat, &parent_meta_head, &temppage,page_pos,child_mode);

  printf("debug dir_add_entry page_pos 2 %ld\n",page_pos);

  return ret_val;
 }


int dir_remove_entry(ino_t parent_inode, ino_t child_inode, char *childname, mode_t child_mode)
 {
  struct stat parent_meta_stat;
  DIR_META_TYPE parent_meta_head;
  DIR_ENTRY_PAGE temppage;
  int ret_items;
  off_t nextfilepos,oldfilepos;
  long page_pos;
  int count,ret_val;

  ret_val = meta_cache_lookup_dir_data(parent_inode, &parent_meta_stat,&parent_meta_head,NULL,0,child_mode);

  ret_val = meta_cache_seek_dir_entry(parent_inode,&temppage,&page_pos, &count, childname, child_mode);

  if ((ret_val ==0) && (count>=0))
   {
    /*Found the entry. Delete it*/
    if (count!=(temppage.num_entries-1))
     memcpy(&(temppage.dir_entries[count]), &(temppage.dir_entries[temppage.num_entries-1]), sizeof(DIR_ENTRY));
    temppage.num_entries--;
    memset(&(temppage.dir_entries[temppage.num_entries]),0,sizeof(DIR_ENTRY));

    /*If the new entry is a subdir, decrease the hard link of the parent*/

    if (child_mode & S_IFDIR)
     parent_meta_stat.st_nlink--;
 
    parent_meta_head.total_children--;
    printf("TOTAL CHILDREN is now %ld\n",parent_meta_head.total_children);
    ret_val = meta_cache_update_dir_data(parent_inode, &parent_meta_stat, &parent_meta_head, &temppage,page_pos,child_mode);
    return 0;
   }

  return -1;
 }

int dir_replace_name(ino_t parent_inode, ino_t child_inode, char *oldname, char *newname, mode_t child_mode)
 {
  DIR_META_TYPE parent_meta_head;
  DIR_ENTRY_PAGE temppage;
  int ret_items;
  off_t nextfilepos,oldfilepos;
  long page_pos;
  int count, ret_val;

  ret_val = meta_cache_seek_dir_entry(parent_inode,&temppage,&page_pos, &count, oldname, child_mode);

  if ((ret_val ==0) && (count>=0))
   {
    /*Found the entry. Replace it*/
    strcpy(temppage.dir_entries[count].d_name,newname);
    ret_val = meta_cache_update_dir_data(parent_inode, NULL, NULL, &temppage,page_pos,child_mode);
    return 0;
   }

  return -1;
 }

int change_parent_inode(ino_t self_inode, ino_t parent_inode1, ino_t parent_inode2)
 {
  DIR_META_TYPE self_meta_head;
  DIR_ENTRY_PAGE temppage;
  int ret_items;
  long page_pos;
  int count;
  int ret_val;

  ret_val = meta_cache_lookup_dir_data(self_inode, NULL,&self_meta_head,NULL,0,0);

  ret_val = meta_cache_seek_dir_entry(self_inode,&temppage,&page_pos, &count, "..", S_IFDIR);

  if ((ret_val ==0) && (count>=0))
   {
    /*Found the entry. Change parent inode*/
    temppage.dir_entries[count].d_ino = parent_inode2;
    ret_val = meta_cache_update_dir_data(self_inode, NULL, NULL, &temppage,page_pos, S_IFDIR);
    return 0;
   }

  return -1;
 }

int decrease_nlink_inode_file(ino_t this_inode)
 {
  char todelete_metapath[METAPATHLEN];
  char thismetapath[METAPATHLEN];
  char thisblockpath[400];
  char filebuf[5000];
  struct stat this_inode_stat;
  FILE *todeletefptr, *metafptr;
  int ret_val;
  long long count;
  long long total_blocks;
  off_t cache_block_size;
  size_t read_size;

  ret_val = meta_cache_lookup_dir_data(this_inode, &this_inode_stat,NULL,NULL,0,0);

  if (this_inode_stat.st_nlink<=1)
   {
    /*Need to delete the inode*/
    super_inode_to_delete(this_inode);
    fetch_todelete_path(todelete_metapath,this_inode);
    fetch_meta_path(thismetapath,this_inode);
    /*Try a rename first*/
    ret_val = rename(thismetapath,todelete_metapath);
    if (ret_val < 0)
     {
      /*If not successful, copy the meta*/
      unlink(todelete_metapath);
      todeletefptr = fopen(todelete_metapath,"w");
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

    ret_val = meta_cache_remove(this_inode);
   }
  else
   {
    this_inode_stat.st_nlink--; 
    ret_val = meta_cache_update_dir_data(this_inode, &this_inode_stat, NULL, NULL,0,0);
   }

  return 0;
 }    

