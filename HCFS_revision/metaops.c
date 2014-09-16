#include <sys/file.h>
#include "global.h"
#include "utils.h"
#include "fuseop.h"
#include "file_present.h"
#include "params.h"
#include "dir_entry_btree.h"
#include "meta_mem_cache.h"
/*TODO: Will need to check if need to explicitly change st_atime, st_mtime*/
/* TODO: Need to consider directory access rights here or in fuseop */
/* TODO: Consider the need to lock meta cache entry between related cache lookup and updates */

int init_dir_page(DIR_ENTRY_PAGE *temppage, ino_t self_inode, ino_t parent_inode, long long this_page_pos)
 {
  memset(temppage,0,sizeof(DIR_ENTRY_PAGE));

  temppage->num_entries = 2;
  (temppage->dir_entries[0]).d_ino = self_inode;
  strcpy((temppage->dir_entries[0]).d_name,".");
  (temppage->dir_entries[0]).d_type = D_ISDIR;

  (temppage->dir_entries[1]).d_ino = parent_inode;
  strcpy((temppage->dir_entries[1]).d_name,"..");
  (temppage->dir_entries[1]).d_type = D_ISDIR;
  temppage->this_page_pos = this_page_pos;
  return 0;
 }

int dir_add_entry(ino_t parent_inode, ino_t child_inode, char *childname, mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr)
 {
  struct stat parent_meta_stat;
  DIR_META_TYPE parent_meta_head;
  DIR_ENTRY_PAGE temppage, new_root, temp_page2;
  DIR_ENTRY temp_entry, overflow_entry;
  long long overflow_new_page;
  int ret_items;
  int ret_val;
  long page_pos;
  int entry_index;

  memset(&temp_entry, 0, sizeof(DIR_ENTRY));
  memset(&overflow_entry, 0, sizeof(DIR_ENTRY));

  temp_entry.d_ino = child_inode;
  strcpy(temp_entry.d_name, childname);
  if (S_ISREG(child_mode))
   temp_entry.d_type = D_ISREG;
  if (S_ISDIR(child_mode))
   temp_entry.d_type = D_ISDIR;

  ret_val = meta_cache_lookup_dir_data(parent_inode, &parent_meta_stat, &parent_meta_head, NULL, body_ptr);

  temppage.this_page_pos = parent_meta_head.root_entry_page;

  ret_val = meta_cache_open_file(body_ptr);

  fseek(body_ptr->fptr, parent_meta_head.root_entry_page, SEEK_SET);
  fread(&temppage, sizeof(DIR_ENTRY_PAGE),1, body_ptr->fptr);

  /* Drop all cached pages first before inserting */
  /* TODO: Future changes could remove this limitation if can update cache with each node change in b-tree*/

  ret_val = meta_cache_drop_pages(body_ptr);

/* B-tree insertion*/
  ret_val = insert_dir_entry_btree(&temp_entry, &temppage, body_ptr->fptr, &overflow_entry, &overflow_new_page, &parent_meta_head);

  if (ret_val == 1)
   {
    /*Need to create a new root page and write to disk*/
    if (parent_meta_head.entry_page_gc_list != 0)
     {
      /*Reclaim node from gc list first*/
      fseek(body_ptr->fptr, parent_meta_head.entry_page_gc_list, SEEK_SET);
      fread(&new_root,sizeof(DIR_ENTRY_PAGE),1,body_ptr->fptr);
      new_root.this_page_pos = parent_meta_head.entry_page_gc_list;
      parent_meta_head.entry_page_gc_list = new_root.gc_list_next;
     }
    else
     {
      memset(&new_root,0,sizeof(DIR_ENTRY_PAGE));
      fseek(body_ptr->fptr,0,SEEK_END);
      new_root.this_page_pos = ftell(body_ptr->fptr);
     }

    new_root.gc_list_next = 0;
    new_root.tree_walk_next = parent_meta_head.tree_walk_list_head;
    new_root.tree_walk_prev = 0;
    fseek(body_ptr->fptr, parent_meta_head.tree_walk_list_head, SEEK_SET);
    fread(&temp_page2, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);
    temp_page2.tree_walk_prev = new_root.this_page_pos;
    fseek(body_ptr->fptr, parent_meta_head.tree_walk_list_head, SEEK_SET);
    fwrite(&temp_page2, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);
    parent_meta_head.tree_walk_list_head = new_root.this_page_pos;

    /* Parent of new node is the same as the parent of the old node*/
    new_root.parent_page_pos = 0;
    memset(new_root.child_page_pos,0,sizeof(long long)*(MAX_DIR_ENTRIES_PER_PAGE+1));
    new_root.num_entries = 1;
    memcpy(&(new_root.dir_entries[0]),&overflow_entry, sizeof(DIR_ENTRY));
    new_root.child_page_pos[0]= parent_meta_head.root_entry_page;
    new_root.child_page_pos[1]= overflow_new_page;

    parent_meta_head.root_entry_page = new_root.this_page_pos;
    /* Write to disk after finishing */
    fseek(body_ptr->fptr, new_root.this_page_pos, SEEK_SET);
    fwrite(&new_root,sizeof(DIR_ENTRY_PAGE),1,body_ptr->fptr);
    fseek(body_ptr->fptr,sizeof(struct stat), SEEK_SET);
    fwrite(&parent_meta_head,sizeof(DIR_META_TYPE),1,body_ptr->fptr);
   }

  /*If the new entry is a subdir, increase the hard link of the parent*/

  if (child_mode & S_IFDIR)
   {
    parent_meta_stat.st_nlink++;
   }

  parent_meta_head.total_children++;
  printf("TOTAL CHILDREN is now %ld\n",parent_meta_head.total_children);

  /* Stat may be dirty after the operation so should write them back to cache*/
  ret_val = meta_cache_update_dir_data(parent_inode, &parent_meta_stat, &parent_meta_head, NULL, body_ptr);

  printf("debug dir_add_entry page_pos 2 %ld\n",page_pos);

  return ret_val;
 }


int dir_remove_entry(ino_t parent_inode, ino_t child_inode, char *childname, mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr)
 {
  struct stat parent_meta_stat;
  DIR_META_TYPE parent_meta_head;
  DIR_ENTRY_PAGE temppage;
  int ret_items;
  off_t nextfilepos,oldfilepos;
  long page_pos;
  int count,ret_val;

  return -1;  /*TODO: REMOVE THIS REMOVE THIS REMOVE THIS REMOVE THIS */

  ret_val = meta_cache_lookup_dir_data(parent_inode, &parent_meta_stat,&parent_meta_head,NULL,body_ptr);
/* TODO: Replace with btree delete routine */
  /* TODO: Drop all cached pages first before inserting or deleting*/
  /* TODO: Future changes could remove this limitation if can update cache with each node change in b-tree*/

  ret_val = meta_cache_seek_dir_entry(parent_inode,&temppage, &count, childname, body_ptr);

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
    ret_val = meta_cache_update_dir_data(parent_inode, &parent_meta_stat, &parent_meta_head, &temppage, body_ptr);
    return 0;
   }

  return -1;
 }


int change_parent_inode(ino_t self_inode, ino_t parent_inode1, ino_t parent_inode2, META_CACHE_ENTRY_STRUCT *body_ptr)
 {
  DIR_META_TYPE self_meta_head;
  DIR_ENTRY_PAGE temppage;
  int ret_items;
  int count;
  int ret_val;

  ret_val = meta_cache_seek_dir_entry(self_inode,&temppage, &count, "..", body_ptr);

  if ((ret_val ==0) && (count>=0))
   {
    /*Found the entry. Change parent inode*/
    temppage.dir_entries[count].d_ino = parent_inode2;
    ret_val = meta_cache_update_dir_data(self_inode, NULL, NULL, &temppage, body_ptr);
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
  META_CACHE_ENTRY_STRUCT *body_ptr;

  body_ptr = meta_cache_lock_entry(this_inode);
  ret_val = meta_cache_lookup_dir_data(this_inode, &this_inode_stat,NULL,NULL, body_ptr);

  if (this_inode_stat.st_nlink<=1)
   {
    ret_val = meta_cache_unlock_entry(this_inode, body_ptr);

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
    ret_val = meta_cache_update_dir_data(this_inode, &this_inode_stat, NULL, NULL,body_ptr);
    ret_val = meta_cache_unlock_entry(this_inode, body_ptr);
   }

  return 0;
 }    

