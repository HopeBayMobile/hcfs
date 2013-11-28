#define FUSE_USE_VERSION 26
#include "fuseop.h"
#include "dir_lookup.h"
#include "super_inode.h"
#include "params.h"
#include <fuse.h>
#include <time.h>
#include <math.h>
#include <sys/statvfs.h>
#include "hcfscurl.h"
#include "hcfs_tocloud.h"


/* TODO: Need to go over the access rights problem for the ops */
/*TODO: Need to invalidate cache entry if rename/deleted */
/*TODO: Need to revisit the error handling in all operations */
/*TODO: Will need to implement rollback or error marking when ops failed*/
/*TODO: Should consider using multiple FILE handler/pointer in one opened file. Could be used for multiple blocks or for a single block for multiple read ops*/
/*TODO: Should handle updating number of blocks and other uncovered info in an inode*/

/*TODO: Will need to fix the cache size prob if a cache entry is opened for writing and then deleted before the opened entry is closed*/


long check_file_size(const char *path)
 {
  struct stat block_stat;

  if (stat(path,&block_stat)==0)
   return block_stat.st_size;
  else
   return -1;
 }

static int hfuse_getattr(const char *path, struct stat *inode_stat)
 {
  ino_t hit_inode;
  SUPER_INODE_ENTRY tempentry;
  int ret_code;

  hit_inode = lookup_pathname(path);

  if (hit_inode > 0)
   {
    ret_code =super_inode_read(hit_inode, &tempentry);    
    if (ret_code < 0)
     return -ENOENT;
    memcpy(inode_stat,&(tempentry.inode_stat),sizeof(struct stat));
    printf("getattr %ld\n",inode_stat->st_ino);
    return 0;
   }
  else
   return -ENOENT;
 }

//int hfuse_readlink(const char *path, char *buf, size_t buf_size);

static int hfuse_mknod(const char *path, mode_t mode, dev_t dev)
 {
  char *parentname;
  char selfname[400];
  char thismetapath[400];
  ino_t self_inode, parent_inode;
  struct stat this_stat;
  FILE_META_TYPE this_meta;
  mode_t self_mode;
  FILE *metafptr;
  int ret_val;

  parentname = malloc(strlen(path)*sizeof(char));
  parse_parent_self(path,parentname,selfname);

  parent_inode = lookup_pathname(parentname);

  free(parentname);
  if (parent_inode < 1)
   return -ENOENT;

  memset(&this_stat,0,sizeof(struct stat));
  memset(&this_meta,0,sizeof(FILE_META_TYPE));

  self_mode = mode | S_IFREG;
  this_stat.st_mode = self_mode;
  this_stat.st_size = 0;
  this_stat.st_blksize = MAX_BLOCK_SIZE;
  this_stat.st_blocks = 0;
  this_stat.st_dev = dev;
  this_stat.st_nlink = 1;
  this_stat.st_uid = getuid();
  this_stat.st_gid = getgid();
  this_stat.st_atime = time(NULL);
  this_stat.st_mtime = this_stat.st_atime;
  this_stat.st_ctime = this_stat.st_ctime;

  self_inode = super_inode_new_inode(&this_stat);
  if (self_inode < 1)
   return -EACCES;
  this_stat.st_ino = self_inode;
  memcpy(&(this_meta.thisstat),&this_stat,sizeof(struct stat));

  fetch_meta_path(thismetapath,self_inode);

  metafptr = fopen(thismetapath,"w");

  if (metafptr == NULL)
   return -EACCES;

  ret_val = fwrite(&this_meta,sizeof(FILE_META_TYPE),1,metafptr);
  fclose(metafptr);
  if (ret_val < 1)
   return -EACCES;

  ret_val = dir_add_entry(parent_inode, self_inode, selfname,self_mode);
  if (ret_val < 0)
   return -EACCES;

  super_inode_mark_dirty(self_inode);

  return 0;
 }
static int hfuse_mkdir(const char *path, mode_t mode)
 {
  char *parentname;
  char selfname[400];
  char thismetapath[400];
  ino_t self_inode, parent_inode;
  struct stat this_stat;
  DIR_META_TYPE this_meta;
  DIR_ENTRY_PAGE temppage;
  mode_t self_mode;
  FILE *metafptr;
  int ret_val;

  parentname = malloc(strlen(path)*sizeof(char));
  parse_parent_self(path,parentname,selfname);

  parent_inode = lookup_pathname(parentname);

  free(parentname);
  if (parent_inode < 1)
   return -ENOENT;

  memset(&this_stat,0,sizeof(struct stat));
  memset(&this_meta,0,sizeof(DIR_META_TYPE));
  memset(&temppage,0,sizeof(DIR_ENTRY_PAGE));

  self_mode = mode | S_IFDIR;
  this_stat.st_mode = self_mode;
  this_stat.st_nlink = 2;   /*One pointed by the parent, another by self*/
  this_stat.st_uid = getuid();
  this_stat.st_gid = getgid();
  this_stat.st_atime = time(NULL);
  this_stat.st_mtime = this_stat.st_atime;
  this_stat.st_ctime = this_stat.st_ctime;
  this_stat.st_size = 0;
  this_stat.st_blksize = MAX_BLOCK_SIZE;
  this_stat.st_blocks = 0;

  self_inode = super_inode_new_inode(&this_stat);
  if (self_inode < 1)
   return -EACCES;
  this_stat.st_ino = self_inode;
  memcpy(&(this_meta.thisstat),&this_stat,sizeof(struct stat));

  fetch_meta_path(thismetapath,self_inode);

  metafptr = fopen(thismetapath,"w");

  if (metafptr == NULL)
   return -EACCES;

  ret_val = fwrite(&this_meta,sizeof(DIR_META_TYPE),1,metafptr);

  if (ret_val < 1)
   {
    fclose(metafptr);
    return -EACCES;
   }
  this_meta.next_subdir_page = ftell(metafptr);
  fseek(metafptr,0,SEEK_SET);

  ret_val = fwrite(&this_meta,sizeof(DIR_META_TYPE),1,metafptr);

  if (ret_val < 1)
   {
    fclose(metafptr);
    return -EACCES;
   }
  temppage.num_entries = 2;
  temppage.dir_entries[0].d_ino = self_inode;
  temppage.dir_entries[1].d_ino = parent_inode;
  strcpy(temppage.dir_entries[0].d_name,".");
  strcpy(temppage.dir_entries[1].d_name,"..");

  ret_val = fwrite(&temppage,sizeof(DIR_ENTRY_PAGE),1,metafptr);
  fclose(metafptr);

  if (ret_val < 1)
   return -EACCES;

  ret_val = dir_add_entry(parent_inode, self_inode, selfname,self_mode);
  if (ret_val < 0)
   return -EACCES;

  super_inode_mark_dirty(self_inode);

  return 0;
 }


int hfuse_unlink(const char *path)
 {
  char *parentname;
  char selfname[400];
  char thismetapath[400];
  ino_t this_inode, parent_inode;
  int ret_val;

  this_inode = lookup_pathname(path);
  if (this_inode < 1)
   return -ENOENT;

  parentname = malloc(strlen(path)*sizeof(char));
  parse_parent_self(path,parentname,selfname);

  parent_inode = lookup_pathname(parentname);

  parse_parent_self(path,parentname,selfname);

  free(parentname);
  if (parent_inode < 1)
   return -ENOENT;

  invalidate_cache_entry(path);

  ret_val = dir_remove_entry(parent_inode,this_inode,selfname,S_IFREG);
  if (ret_val < 0)
   return -EACCES;

  ret_val = decrease_nlink_inode_file(this_inode);

  return ret_val;
 }


int hfuse_rmdir(const char *path)
 {
  char *parentname;
  char selfname[400];
  char thismetapath[400];
  ino_t this_inode, parent_inode;
  int ret_val;
  FILE *metafptr;
  DIR_META_TYPE tempmeta;

  this_inode = lookup_pathname(path);
  if (this_inode < 1)
   return -ENOENT;

  parentname = malloc(strlen(path)*sizeof(char));
  parse_parent_self(path,parentname,selfname);

  parent_inode = lookup_pathname(parentname);

  parse_parent_self(path,parentname,selfname);

  free(parentname);
  if (!strcmp(selfname,"."))
   return -EINVAL;
  if (!strcmp(selfname,".."))
   return -ENOTEMPTY;

  if (parent_inode < 1)
   return -ENOENT;

  invalidate_cache_entry(path);

  fetch_meta_path(thismetapath,this_inode);

  metafptr = fopen(thismetapath,"r+");

  if (metafptr == NULL)
   return -EACCES;
  setbuf(metafptr,NULL);
  flock(fileno(metafptr),LOCK_EX);
  fread(&tempmeta,sizeof(DIR_META_TYPE),1,metafptr);
  printf("TOTAL CHILDREN is now %ld\n",tempmeta.total_children);

  if (tempmeta.total_children > 0)
   {
    flock(fileno(metafptr),LOCK_UN);
    fclose(metafptr);
    return -ENOTEMPTY;
   }

  ret_val = dir_remove_entry(parent_inode,this_inode,selfname,S_IFDIR);
  if (ret_val < 0)
   {
    flock(fileno(metafptr),LOCK_UN);
    fclose(metafptr);
    return -EACCES;
   }

  /*Need to delete the inode*/
  /*TODO: queue inode as to delete. Now will jump to delete*/
  /*TODO: Schedule backend meta object for deletion*/

  unlink(thismetapath);
  ftruncate(fileno(metafptr),0);
  super_inode_delete(this_inode);
  super_inode_reclaim();

  flock(fileno(metafptr),LOCK_UN);
  fclose(metafptr);

  return ret_val;
 }

//int hfuse_symlink(const char *oldpath, const char *newpath);


static int hfuse_rename(const char *oldpath, const char *newpath)
 {
  char *parentname1;
  char selfname1[400];
  char *parentname2;
  char selfname2[400];
  ino_t parent_inode1,parent_inode2,self_inode;
  int ret_val;
  SUPER_INODE_ENTRY tempentry;
  mode_t self_mode;

  self_inode = lookup_pathname(oldpath);
  if (self_inode < 1)
   return -ENOENT;

  invalidate_cache_entry(oldpath);

  if (lookup_pathname(newpath) > 0)
   return -EACCES;

  ret_val =super_inode_read(self_inode, &tempentry);    
  if (ret_val < 0)
   return -ENOENT;

  self_mode = tempentry.inode_stat.st_mode;

  /*TODO: Will now only handle simple types (that the target is empty and no symlinks)*/
  parentname1 = malloc(strlen(oldpath)*sizeof(char));
  parentname2 = malloc(strlen(newpath)*sizeof(char));
  parse_parent_self(oldpath,parentname1,selfname1);
  parse_parent_self(newpath,parentname2,selfname2);

  parent_inode1 = lookup_pathname(parentname1);
  parent_inode2 = lookup_pathname(parentname2);

  free(parentname1);
  free(parentname2);
  if ((parent_inode1 < 1) || (parent_inode2 < 1))
   return -ENOENT;
  

  if (parent_inode1 == parent_inode2)
   {
    ret_val = dir_replace_name(parent_inode1, self_inode, selfname1, selfname2, self_mode);
    if (ret_val < 0)
     return -EACCES;
    return 0;
   }

  ret_val = dir_remove_entry(parent_inode1,self_inode,selfname1,self_mode);
  if (ret_val < 0)
   return -EACCES;

  ret_val = dir_add_entry(parent_inode2,self_inode,selfname2,self_mode);

  if (ret_val < 0)
   return -EACCES;


  if ((self_mode & S_IFDIR) && (parent_inode1 != parent_inode2))
   {
    ret_val = change_parent_inode(self_inode, parent_inode1, parent_inode2);
    if (ret_val < 0)
     return -EACCES;
   }
  return 0;
 }

//int hfuse_link(const char *oldpath, const char *newpath);
int hfuse_chmod(const char *path, mode_t mode)
 {
  SUPER_INODE_ENTRY tempentry;
  FILE_META_TYPE tempfilemeta;
  DIR_META_TYPE tempdirmeta;
  int ret_val;
  ino_t this_inode;
  char thismetapath[1024];
  FILE *fptr;

  printf("Debug chmod\n");
  this_inode = lookup_pathname(path);
  if (this_inode < 1)
   return -ENOENT;

  fetch_meta_path(thismetapath,this_inode);
  printf("%ld %s\n",this_inode,thismetapath);
  fptr = fopen(thismetapath,"r+");
  if (fptr==NULL)
   return -ENOENT;
  setbuf(fptr,NULL);
  
  super_inode_read(this_inode, &tempentry);

  flock(fileno(fptr),LOCK_EX);
  if (tempentry.inode_stat.st_mode & S_IFREG)
   {
    fread(&tempfilemeta,sizeof(FILE_META_TYPE),1,fptr);
    tempfilemeta.thisstat.st_mode = mode;
    tempfilemeta.thisstat.st_ctime = time(NULL);
    fseek(fptr,0,SEEK_SET);
    fwrite(&tempfilemeta,sizeof(FILE_META_TYPE),1,fptr);
    memcpy(&(tempentry.inode_stat),&(tempfilemeta.thisstat),sizeof(struct stat));
    flock(fileno(fptr),LOCK_UN);
    fclose(fptr);
    super_inode_write(this_inode, &tempentry);
   }  
  else
   {
    if (tempentry.inode_stat.st_mode & S_IFDIR)
     {
      fread(&tempdirmeta,sizeof(DIR_META_TYPE),1,fptr);
      tempfilemeta.thisstat.st_mode = mode;
      tempdirmeta.thisstat.st_ctime = time(NULL);
      fseek(fptr,0,SEEK_SET);
      fwrite(&tempdirmeta,sizeof(DIR_META_TYPE),1,fptr);
      memcpy(&(tempentry.inode_stat),&(tempdirmeta.thisstat),sizeof(struct stat));
      flock(fileno(fptr),LOCK_UN);
      fclose(fptr);
      super_inode_write(this_inode, &tempentry);
     } 
    else
     {
      /*TODO: Handle symlink in the future */
      flock(fileno(fptr),LOCK_UN);
      fclose(fptr);
     }
   }
  return 0;
 }

int hfuse_chown(const char *path, uid_t owner, gid_t group)
 {
  SUPER_INODE_ENTRY tempentry;
  FILE_META_TYPE tempfilemeta;
  DIR_META_TYPE tempdirmeta;
  int ret_val;
  ino_t this_inode;
  char thismetapath[1024];
  FILE *fptr;

  this_inode = lookup_pathname(path);
  if (this_inode < 1)
   return -ENOENT;

  fetch_meta_path(thismetapath,this_inode);
  fptr = fopen(thismetapath,"r+");
  if (fptr==NULL)
   return -ENOENT;
  setbuf(fptr,NULL);
  
  super_inode_read(this_inode, &tempentry);

  flock(fileno(fptr),LOCK_EX);
  if (tempentry.inode_stat.st_mode & S_IFREG)
   {
    fread(&tempfilemeta,sizeof(FILE_META_TYPE),1,fptr);
    tempfilemeta.thisstat.st_uid = owner;
    tempfilemeta.thisstat.st_gid = group;
    tempfilemeta.thisstat.st_ctime = time(NULL);
    fseek(fptr,0,SEEK_SET);
    fwrite(&tempfilemeta,sizeof(FILE_META_TYPE),1,fptr);
    memcpy(&(tempentry.inode_stat),&(tempfilemeta.thisstat),sizeof(struct stat));
    flock(fileno(fptr),LOCK_UN);
    fclose(fptr);
    super_inode_write(this_inode, &tempentry);
   }  
  else
   {
    if (tempentry.inode_stat.st_mode & S_IFDIR)
     {
      fread(&tempdirmeta,sizeof(DIR_META_TYPE),1,fptr);
      tempdirmeta.thisstat.st_uid = owner;
      tempdirmeta.thisstat.st_gid = group;
      tempdirmeta.thisstat.st_ctime = time(NULL);
      fseek(fptr,0,SEEK_SET);
      fwrite(&tempdirmeta,sizeof(DIR_META_TYPE),1,fptr);
      memcpy(&(tempentry.inode_stat),&(tempdirmeta.thisstat),sizeof(struct stat));
      flock(fileno(fptr),LOCK_UN);
      fclose(fptr);
      super_inode_write(this_inode, &tempentry);
     } 
    else
     {
      /*TODO: Handle symlink in the future */
      flock(fileno(fptr),LOCK_UN);
      fclose(fptr);
     }
   }
  return 0;
 }


int hfuse_truncate(const char *path, off_t offset)
 {
/*TODO: If truncate file smaller, do not truncate metafile, but instead set the affected entries to ST_NONE or others*/
  return 0;
 }
int hfuse_open(const char *path, struct fuse_file_info *file_info)
 {
  /*TODO: Need to check permission here*/
  ino_t thisinode;
  long fh;

  thisinode = lookup_pathname(path);
  if (thisinode < 1)
   return -ENOENT;

  fh = open_fh(thisinode);
  if (fh < 0)
   return -ENFILE;

  file_info->fh = fh;

  return 0;
 }

int hfuse_read(const char *path, char *buf, size_t size_org, off_t offset, struct fuse_file_info *file_info)
 {
  FH_ENTRY *fh_ptr;
  long start_block, end_block, current_block;
  long start_page, end_page, current_page;
  long nextfilepos, prevfilepos, currentfilepos;
  BLOCK_ENTRY_PAGE temppage;
  long entry_index;
  long block_index;
  char thisblockpath[400];
  int total_bytes_read;
  int this_bytes_read;
  off_t current_offset;
  int target_bytes_read;
  size_t size;
  char fill_zeros;
  struct stat tempstat;


/*TODO: Perhaps should do proof-checking on the inode number using pathname lookup and from file_info*/
  if (system_fh_table.entry_table_flags[file_info->fh] == FALSE)
   return 0;

  fh_ptr = &(system_fh_table.entry_table[file_info->fh]);

  flockfile(fh_ptr-> metafptr);
  fseek(fh_ptr->metafptr,0,SEEK_SET);
  fread(&(fh_ptr->cached_meta),sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);
  funlockfile(fh_ptr-> metafptr);


  if ((fh_ptr->cached_meta).thisstat.st_size < (offset+size_org))
   size = ((fh_ptr->cached_meta).thisstat.st_size - offset);
  else
   size = size_org;

  if (size <=0)
   return 0;

  total_bytes_read = 0;

  start_block = (offset / MAX_BLOCK_SIZE);  /* Block indexing starts at zero */
  end_block = ((offset+size-1) / MAX_BLOCK_SIZE);

  start_page = start_block / MAX_BLOCK_ENTRIES_PER_PAGE; /*Page indexing starts at zero*/
  end_page = end_block / MAX_BLOCK_ENTRIES_PER_PAGE;

  if (fh_ptr->cached_page_index != start_page)
   {
    flockfile(fh_ptr-> metafptr);
    flock(fileno(fh_ptr-> metafptr),LOCK_EX);
    fseek(fh_ptr->metafptr,0,SEEK_SET);
    fread(&(fh_ptr->cached_meta),sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);

    if (fh_ptr->cached_page_index != start_page)  /*Check if other threads have already done the work*/
     seek_page(fh_ptr-> metafptr,fh_ptr, start_page);
    flock(fileno(fh_ptr->metafptr),LOCK_UN);
    funlockfile(fh_ptr->metafptr);
   }

  entry_index = start_block % MAX_BLOCK_ENTRIES_PER_PAGE;

  for(block_index = start_block; block_index <= end_block; block_index++)
   {
    sem_wait(&(fh_ptr->block_sem));
    if (fh_ptr->opened_block != block_index)
     {
      if (fh_ptr->opened_block != -1)
       {
        fclose(fh_ptr->blockfptr);
        fh_ptr->opened_block = -1;
       }

      flockfile(fh_ptr->metafptr);
      fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
      fread(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
      funlockfile(fh_ptr->metafptr);

      while (((fh_ptr->cached_page).block_entries[entry_index].status == ST_CLOUD) ||
             ((fh_ptr->cached_page).block_entries[entry_index].status == ST_CtoL))
       {
        if (hcfs_system->systemdata.cache_size > CACHE_HARD_LIMIT) /*Sleep if cache already full*/
         {
          sem_post(&(fh_ptr->block_sem));
          printf("debug read waiting on full cache\n");
          sleep_on_cache_full();
          sem_wait(&(fh_ptr->block_sem));
          /*Re-read status*/
          flockfile(fh_ptr->metafptr);
          fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
          fread(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
          funlockfile(fh_ptr->metafptr);
         }
        else
         break;
       }


      switch((fh_ptr->cached_page).block_entries[entry_index].status)
       {
        case ST_NONE: 
            fill_zeros = TRUE;
            break;
        case ST_LDISK:
        case ST_BOTH:
        case ST_LtoC:
            fill_zeros = FALSE;
            break;
        case ST_CLOUD:
        case ST_CtoL:        
            /*Download from backend */
            fetch_block_path(thisblockpath,(fh_ptr->cached_meta).thisstat.st_ino,block_index);
            fh_ptr->blockfptr = fopen(thisblockpath,"a+");
            fclose(fh_ptr->blockfptr);
            fh_ptr->blockfptr = fopen(thisblockpath,"r+");
            setbuf(fh_ptr->blockfptr,NULL);
            flock(fileno(fh_ptr->blockfptr),LOCK_EX);
            flockfile(fh_ptr->metafptr);
            flock(fileno(fh_ptr->metafptr),LOCK_EX);
            fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
            fread(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
            if (((fh_ptr->cached_page).block_entries[entry_index].status == ST_CLOUD) ||
                ((fh_ptr->cached_page).block_entries[entry_index].status == ST_CtoL))
             {
              if ((fh_ptr->cached_page).block_entries[entry_index].status == ST_CLOUD)
               {
                (fh_ptr->cached_page).block_entries[entry_index].status = ST_CtoL;
                fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
                fwrite(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
                fflush(fh_ptr->metafptr);
               }
              flock(fileno(fh_ptr->metafptr),LOCK_UN);
              fetch_from_cloud(fh_ptr->blockfptr,(fh_ptr->cached_meta).thisstat.st_ino,block_index);
              /*Do not process cache update and stored_where change if block is actually deleted by other ops such as truncate*/
              flock(fileno(fh_ptr->metafptr),LOCK_EX);
              fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
              fread(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
              if (stat(thisblockpath,&tempstat)==0)
               {
                (fh_ptr->cached_page).block_entries[entry_index].status = ST_BOTH;
                fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
                fwrite(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
                fflush(fh_ptr->metafptr);

                sem_wait(&(hcfs_system->access_sem));
                hcfs_system->systemdata.cache_size += tempstat.st_size;
                hcfs_system->systemdata.cache_blocks++;
                sync_hcfs_system_data(FALSE);
                sem_post(&(hcfs_system->access_sem));           
               }
             }
            flock(fileno(fh_ptr->metafptr),LOCK_UN);
            funlockfile(fh_ptr->metafptr);
            flock(fileno(fh_ptr->blockfptr),LOCK_UN);
            fclose(fh_ptr->blockfptr);
            fill_zeros = FALSE;
            break;
        default:
            break;
       }

      if (fill_zeros != TRUE)
       {
        fetch_block_path(thisblockpath,(fh_ptr->cached_meta).thisstat.st_ino,block_index);

        fh_ptr->blockfptr=fopen(thisblockpath,"r+");
        setbuf(fh_ptr->blockfptr,NULL);
        fh_ptr->opened_block = block_index;
       }
     }

    if (fill_zeros != TRUE)
     flock(fileno(fh_ptr->blockfptr),LOCK_SH);

    current_offset = (offset+total_bytes_read) % MAX_BLOCK_SIZE;
    target_bytes_read = MAX_BLOCK_SIZE - current_offset;
    if ((size - total_bytes_read) < target_bytes_read) /*Do not need to read that much*/
     target_bytes_read = size - total_bytes_read;

    if (fill_zeros != TRUE)
     {
      fseek(fh_ptr->blockfptr,current_offset,SEEK_SET);
      this_bytes_read = fread(&buf[total_bytes_read],sizeof(char),target_bytes_read, fh_ptr->blockfptr);
      if (this_bytes_read < target_bytes_read)
       {  /*Need to pad zeros*/
        memset(&buf[total_bytes_read + this_bytes_read],0,sizeof(char) * (target_bytes_read - this_bytes_read));
        this_bytes_read = target_bytes_read;
       }
     }
    else
     {
      this_bytes_read = target_bytes_read;
      memset(&buf[total_bytes_read],0,sizeof(char) * target_bytes_read);
     }

    total_bytes_read += this_bytes_read;

    if (fill_zeros != TRUE)
     flock(fileno(fh_ptr->blockfptr),LOCK_UN);
 
    sem_post(&(fh_ptr->block_sem));

    if (this_bytes_read < target_bytes_read) /*Terminate if cannot write as much as we want*/
     break;

    if (block_index < end_block)  /*If this is not the last block, need to advance one more*/
     {
      if ((entry_index+1) >= MAX_BLOCK_ENTRIES_PER_PAGE) /*If may need to change meta, lock*/
       {
        flockfile(fh_ptr-> metafptr);
        flock(fileno(fh_ptr-> metafptr),LOCK_EX);
        advance_block(fh_ptr-> metafptr,fh_ptr,&entry_index);
        flock(fileno(fh_ptr-> metafptr),LOCK_UN);
        funlockfile(fh_ptr-> metafptr);
       }
      else
       entry_index++;
     }
   }

  if (total_bytes_read > 0)
   {
    flockfile(fh_ptr-> metafptr);
    flock(fileno(fh_ptr-> metafptr),LOCK_EX);

    /*Update and flush file meta*/

    fseek(fh_ptr->metafptr,0,SEEK_SET);
    fread(&(fh_ptr->cached_meta),sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);


    if (total_bytes_read > 0)
     (fh_ptr->cached_meta).thisstat.st_atime = time(NULL);

    fseek(fh_ptr->metafptr,0,SEEK_SET);
    fwrite(&(fh_ptr->cached_meta), sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);

    super_inode_update_stat((fh_ptr->cached_meta).thisstat.st_ino, &((fh_ptr->cached_meta).thisstat));

    flock(fileno(fh_ptr-> metafptr),LOCK_UN);
    funlockfile(fh_ptr-> metafptr);
   }

  return total_bytes_read;
 }

int hfuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *file_info)
 {
  FH_ENTRY *fh_ptr;
  long start_block, end_block, current_block;
  long start_page, end_page, current_page;
  long nextfilepos, prevfilepos, currentfilepos;
  BLOCK_ENTRY_PAGE temppage;
  long entry_index;
  long block_index;
  char thisblockpath[400];
  int total_bytes_written;
  int this_bytes_written;
  off_t current_offset;
  int target_bytes_written;
  long old_cache_size, new_cache_size;
  struct stat tempstat;

/*TODO: Perhaps should do proof-checking on the inode number using pathname lookup and from file_info*/

  if (system_fh_table.entry_table_flags[file_info->fh] == FALSE)
   return 0;

  if (size <=0)
   return 0;

  if (hcfs_system->systemdata.cache_size > CACHE_HARD_LIMIT) /*Sleep if cache already full*/
    sleep_on_cache_full();

  total_bytes_written = 0;

  start_block = (offset / MAX_BLOCK_SIZE);  /* Block indexing starts at zero */
  end_block = ((offset+size-1) / MAX_BLOCK_SIZE);

  start_page = start_block / MAX_BLOCK_ENTRIES_PER_PAGE; /*Page indexing starts at zero*/
  end_page = end_block / MAX_BLOCK_ENTRIES_PER_PAGE;

  fh_ptr = &(system_fh_table.entry_table[file_info->fh]);

  flockfile(fh_ptr-> metafptr);
  flock(fileno(fh_ptr-> metafptr),LOCK_EX);

  fseek(fh_ptr->metafptr,0,SEEK_SET);
  fread(&(fh_ptr->cached_meta),sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);


  if (fh_ptr->cached_page_index != start_page)
   seek_page(fh_ptr-> metafptr,fh_ptr, start_page);

  entry_index = start_block % MAX_BLOCK_ENTRIES_PER_PAGE;

  for(block_index = start_block; block_index <= end_block; block_index++)
   {
    /*TODO: For now, only consider storing locally*/
    /*TODO: Need to be able to modify status to locally stored for cases other than ST_NONE*/

    fetch_block_path(thisblockpath,(fh_ptr->cached_meta).thisstat.st_ino,block_index);
    sem_wait(&(fh_ptr->block_sem));
    if (fh_ptr->opened_block != block_index)
     {
      if (fh_ptr->opened_block != -1)
       {
        fclose(fh_ptr->blockfptr);
        fh_ptr->opened_block = -1;
       }
      fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
      fread(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);

      while (((fh_ptr->cached_page).block_entries[entry_index].status == ST_CLOUD) ||
             ((fh_ptr->cached_page).block_entries[entry_index].status == ST_CtoL))
       {
        if (hcfs_system->systemdata.cache_size > CACHE_HARD_LIMIT) /*Sleep if cache already full*/
         {
          sem_post(&(fh_ptr->block_sem));
          flock(fileno(fh_ptr-> metafptr),LOCK_UN);
          funlockfile(fh_ptr-> metafptr);
          printf("debug read waiting on full cache\n");
          sleep_on_cache_full();
          /*Re-read status*/
          flockfile(fh_ptr->metafptr);
          flock(fileno(fh_ptr-> metafptr),LOCK_EX);
          sem_wait(&(fh_ptr->block_sem));
          fseek(fh_ptr->metafptr,0,SEEK_SET);
          fread(&(fh_ptr->cached_meta),sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);
          fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
          fread(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
         }
        else
         break;
       }


      switch((fh_ptr->cached_page).block_entries[entry_index].status)
       {
        case ST_NONE:
             /*If not stored anywhere, make it on local disk*/
            fh_ptr->blockfptr=fopen(thisblockpath,"a+");
            fclose(fh_ptr->blockfptr);
            (fh_ptr->cached_page).block_entries[entry_index].status = ST_LDISK;
            fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
            fwrite(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
            sem_wait(&(hcfs_system->access_sem));
            hcfs_system->systemdata.cache_blocks++;
            sync_hcfs_system_data(FALSE);
            sem_post(&(hcfs_system->access_sem));           
            break;
        case ST_LDISK:
            break;
        case ST_BOTH:
        case ST_LtoC:
            (fh_ptr->cached_page).block_entries[entry_index].status = ST_LDISK;
            fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
            fwrite(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
            break;
        case ST_CLOUD:
        case ST_CtoL:        
            /*Download from backend */
            fetch_block_path(thisblockpath,(fh_ptr->cached_meta).thisstat.st_ino,block_index);
            fh_ptr->blockfptr = fopen(thisblockpath,"a+");
            fclose(fh_ptr->blockfptr);
            fh_ptr->blockfptr = fopen(thisblockpath,"r+");
            setbuf(fh_ptr->blockfptr,NULL);
            flock(fileno(fh_ptr->blockfptr),LOCK_EX);
            fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
            fread(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
            if (((fh_ptr->cached_page).block_entries[entry_index].status == ST_CLOUD) ||
                ((fh_ptr->cached_page).block_entries[entry_index].status == ST_CtoL))
             {
              if ((fh_ptr->cached_page).block_entries[entry_index].status == ST_CLOUD)
               {
                (fh_ptr->cached_page).block_entries[entry_index].status = ST_CtoL;
                fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
                fwrite(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
                fflush(fh_ptr->metafptr);
               }
              flock(fileno(fh_ptr-> metafptr),LOCK_UN);
              fetch_from_cloud(fh_ptr->blockfptr,(fh_ptr->cached_meta).thisstat.st_ino,block_index);
              /*Do not process cache update and stored_where change if block is actually deleted by other ops such as truncate*/

              /*Re-read status*/
              flock(fileno(fh_ptr-> metafptr),LOCK_EX);
              fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
              fread(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);

              if (stat(thisblockpath,&tempstat)==0)
               {
                (fh_ptr->cached_page).block_entries[entry_index].status = ST_LDISK;
                fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
                fwrite(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
                fflush(fh_ptr->metafptr);

                sem_wait(&(hcfs_system->access_sem));
                hcfs_system->systemdata.cache_size += tempstat.st_size;
                hcfs_system->systemdata.cache_blocks++;
                sync_hcfs_system_data(FALSE);
                sem_post(&(hcfs_system->access_sem));           
               }
             }
            flock(fileno(fh_ptr->blockfptr),LOCK_UN);
            fclose(fh_ptr->blockfptr);
            break;
        default:
            break;
       }

      fh_ptr->blockfptr=fopen(thisblockpath,"r+");
      setbuf(fh_ptr->blockfptr,NULL);
      fh_ptr->opened_block = block_index;
     }
    flock(fileno(fh_ptr->blockfptr),LOCK_EX);

    current_offset = (offset+total_bytes_written) % MAX_BLOCK_SIZE;

    old_cache_size = check_file_size(thisblockpath);
    if (current_offset > old_cache_size)
     {
      printf("Debug write: cache block size smaller than starting offset. Extending\n");
      ftruncate(fileno(fh_ptr->blockfptr),current_offset);
     }

    target_bytes_written = MAX_BLOCK_SIZE - current_offset;
    if ((size - total_bytes_written) < target_bytes_written) /*Do not need to write that much*/
     target_bytes_written = size - total_bytes_written;

    fseek(fh_ptr->blockfptr,current_offset,SEEK_SET);
    this_bytes_written = fwrite(&buf[total_bytes_written],sizeof(char),target_bytes_written, fh_ptr->blockfptr);

    total_bytes_written += this_bytes_written;

    new_cache_size = check_file_size(thisblockpath);

    if (old_cache_size != new_cache_size)
     {
      sem_wait(&(hcfs_system->access_sem));
      hcfs_system->systemdata.cache_size += new_cache_size - old_cache_size;
      if (hcfs_system->systemdata.cache_size < 0)
       hcfs_system->systemdata.cache_size = 0;
      sync_hcfs_system_data(FALSE);
      sem_post(&(hcfs_system->access_sem));           
     }

    flock(fileno(fh_ptr->blockfptr),LOCK_UN);
    sem_post(&(fh_ptr->block_sem));

    if (this_bytes_written < target_bytes_written) /*Terminate if cannot write as much as we want*/
     break;

    if (block_index < end_block)  /*If this is not the last block, need to advance one more*/
     advance_block(fh_ptr-> metafptr,fh_ptr,&entry_index);
   }

  /*Update and flush file meta*/

  fseek(fh_ptr->metafptr,0,SEEK_SET);
  fread(&(fh_ptr->cached_meta),sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);

  if ((fh_ptr->cached_meta).thisstat.st_size < (offset + total_bytes_written))
   {
    sem_wait(&(hcfs_system->access_sem));
    hcfs_system->systemdata.system_size += (offset + total_bytes_written) - (fh_ptr->cached_meta).thisstat.st_size;
    sync_hcfs_system_data(FALSE);
    sem_post(&(hcfs_system->access_sem));           

    (fh_ptr->cached_meta).thisstat.st_size = (offset + total_bytes_written);
    (fh_ptr->cached_meta).thisstat.st_blocks = ((fh_ptr->cached_meta).thisstat.st_size +511) / 512;
   }

  if (total_bytes_written > 0)
   (fh_ptr->cached_meta).thisstat.st_mtime = time(NULL);

  fseek(fh_ptr->metafptr,0,SEEK_SET);
  fwrite(&(fh_ptr->cached_meta), sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);

  super_inode_update_stat((fh_ptr->cached_meta).thisstat.st_ino, &((fh_ptr->cached_meta).thisstat));

  flock(fileno(fh_ptr-> metafptr),LOCK_UN);
  funlockfile(fh_ptr-> metafptr);

  return total_bytes_written;
 }
  
int hfuse_statfs(const char *path, struct statvfs *buf)      /*Prototype is linux statvfs call*/
 {

  sem_wait(&(hcfs_system->access_sem));
  buf->f_bsize = 4096;
  buf->f_frsize = 4096;
  if (hcfs_system->systemdata.system_size > (50*powl(1024,3)))
   buf->f_blocks = (2*hcfs_system->systemdata.system_size) / 4096;
  else
   buf->f_blocks = (100*powl(1024,3)) / 4096;

  buf->f_bfree = buf->f_blocks - ((hcfs_system->systemdata.system_size) / 4096);
  if (buf->f_bfree < 0)
   buf->f_bfree = 0;
  buf->f_bavail = buf->f_bfree;
  sem_post(&(hcfs_system->access_sem));  

  sem_wait(&(sys_super_inode->io_sem));
  if (sys_super_inode->head.num_active_inodes > 1000000)
   buf->f_files = (sys_super_inode->head.num_active_inodes * 2);
  else
   buf->f_files = 2000000;

  buf->f_ffree = buf->f_files - sys_super_inode->head.num_active_inodes;
  if (buf->f_ffree < 0)
   buf->f_ffree = 0;
  buf->f_favail = buf->f_ffree;
  sem_post(&(sys_super_inode->io_sem));
  buf->f_namemax = 256;

  return 0;
 }
int hfuse_flush(const char *path, struct fuse_file_info *file_info)
 {
  return 0;
 }
int hfuse_release(const char *path, struct fuse_file_info *file_info)
 {
  ino_t thisinode;

  thisinode = lookup_pathname(path);
  if (file_info->fh < 0)
   return -EBADF;

  if (file_info->fh >= MAX_OPEN_FILE_ENTRIES)
   return -EBADF;

  if (system_fh_table.entry_table_flags[file_info->fh] == FALSE)
   return -EBADF;

  if (system_fh_table.entry_table[file_info->fh].thisinode != thisinode)
   return -EBADF;

  close_fh(file_info->fh);
  return 0;
 }
int hfuse_fsync(const char *path, int isdatasync, struct fuse_file_info *file_info)
 {
  return 0;
 }
//int hfuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
//int hfuse_getxattr(const char *path, const char *name, char *value, size_t size);
//int hfuse_listxattr(const char *path, char *list, size_t size);
//int hfuse_removexattr(const char *path, const char *name);


static int hfuse_opendir(const char *path, struct fuse_file_info *file_info)
 {
  /*TODO: Need to check for access rights */
  return 0;
 }
static int hfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *file_info)
 {
  ino_t this_inode;
  char pathname[400];
  FILE *fptr;
  int count;
  long thisfile_pos;
  DIR_META_TYPE tempmeta;
  DIR_ENTRY_PAGE temp_page;
  struct stat tempstat;

/*TODO: Need to include symlinks*/
/*TODO: Will need to test the boundary of the operation. When will buf run out of space?*/
  fprintf(stderr,"DEBUG readdir entering readdir\n");

  this_inode = lookup_pathname(path);

  if (this_inode == 0)
   return -ENOENT;

  fetch_meta_path(pathname,this_inode);
  fptr = fopen(pathname,"r");
  setbuf(fptr,NULL);
  flock(fileno(fptr),LOCK_SH);

  fread(&tempmeta,sizeof(DIR_META_TYPE),1,fptr);
  thisfile_pos = tempmeta.next_subdir_page;

  while(thisfile_pos != 0)
   {
    fseek(fptr, thisfile_pos, SEEK_SET);
    fread(&temp_page,sizeof(DIR_ENTRY_PAGE),1,fptr);

    for(count=0;count<temp_page.num_entries;count++)
     {
      tempstat.st_ino = temp_page.dir_entries[count].d_ino;
      tempstat.st_mode = S_IFDIR;
      if (filler(buf,temp_page.dir_entries[count].d_name, &tempstat,0))
       {
        flock(fileno(fptr),LOCK_UN);
        fclose(fptr);
        return 0;
       }
     }
    thisfile_pos = temp_page.next_page;
   }

  thisfile_pos = tempmeta.next_file_page;
  while(thisfile_pos != 0)
   {
    fseek(fptr, thisfile_pos, SEEK_SET);
    fread(&temp_page,sizeof(DIR_ENTRY_PAGE),1,fptr);
    for (count=0;count<temp_page.num_entries;count++)
     {
      tempstat.st_ino = temp_page.dir_entries[count].d_ino;
      tempstat.st_mode = S_IFREG;
      if (filler(buf,temp_page.dir_entries[count].d_name, &tempstat,0))
       {
        flock(fileno(fptr),LOCK_UN);
        fclose(fptr);
        return 0;
       }
     }
    thisfile_pos = temp_page.next_page;
   }
  flock(fileno(fptr),LOCK_UN);
  fclose(fptr);
  return 0;
 }

int hfuse_releasedir(const char *path, struct fuse_file_info *file_info)
 {
  return 0;
 }
void* hfuse_init(struct fuse_conn_info *conn)
 {
  return ((void*) sys_super_inode);
 }
void hfuse_destroy(void *private_data)
 {
  int download_handle_count;

  for(download_handle_count=0;download_handle_count<MAX_DOWNLOAD_CURL_HANDLE;download_handle_count++)
   {
    hcfs_destroy_swift_backend(download_curl_handles[download_handle_count].curl);
   }
  fclose(logfptr);

  return;
 }
//int hfuse_create(const char *path, mode_t mode, struct fuse_file_info *file_info);
static int hfuse_access(const char *path, int mode)
 {
  /*TODO: finish this*/
  return 0;
 }

static int hfuse_utimens(const char *path, const struct timespec tv[2])
 {
  SUPER_INODE_ENTRY tempentry;
  FILE_META_TYPE tempfilemeta;
  DIR_META_TYPE tempdirmeta;
  int ret_val;
  ino_t this_inode;
  char thismetapath[1024];
  FILE *fptr;

  this_inode = lookup_pathname(path);
  if (this_inode < 1)
   return -ENOENT;

  fetch_meta_path(thismetapath,this_inode);
  fptr = fopen(thismetapath,"r+");
  if (fptr==NULL)
   return -ENOENT;
  setbuf(fptr,NULL);
  
  super_inode_read(this_inode, &tempentry);

  flock(fileno(fptr),LOCK_EX);
  if (tempentry.inode_stat.st_mode & S_IFREG)
   {
    fread(&tempfilemeta,sizeof(FILE_META_TYPE),1,fptr);
    tempfilemeta.thisstat.st_atime = (time_t)(tv[0].tv_sec);
    tempfilemeta.thisstat.st_mtime = (time_t)(tv[1].tv_sec);
    fseek(fptr,0,SEEK_SET);
    fwrite(&tempfilemeta,sizeof(FILE_META_TYPE),1,fptr);
    memcpy(&(tempentry.inode_stat),&(tempfilemeta.thisstat),sizeof(struct stat));
    flock(fileno(fptr),LOCK_UN);
    fclose(fptr);
    super_inode_write(this_inode, &tempentry);
   }  
  else
   {
    if (tempentry.inode_stat.st_mode & S_IFDIR)
     {
      fread(&tempdirmeta,sizeof(DIR_META_TYPE),1,fptr);
      tempfilemeta.thisstat.st_atime = (time_t)(tv[0].tv_sec);
      tempfilemeta.thisstat.st_mtime = (time_t)(tv[1].tv_sec);
      fseek(fptr,0,SEEK_SET);
      fwrite(&tempdirmeta,sizeof(DIR_META_TYPE),1,fptr);
      memcpy(&(tempentry.inode_stat),&(tempdirmeta.thisstat),sizeof(struct stat));
      flock(fileno(fptr),LOCK_UN);
      fclose(fptr);
      super_inode_write(this_inode, &tempentry);
     } 
    else
     {
      /*TODO: Handle symlink in the future */
      flock(fileno(fptr),LOCK_UN);
      fclose(fptr);
     }
   }
  return 0;
 }


static struct fuse_operations hfuse_ops = {
    .getattr = hfuse_getattr,
    .mknod = hfuse_mknod,
    .mkdir = hfuse_mkdir,
    .readdir = hfuse_readdir,
    .opendir = hfuse_opendir,
    .access = hfuse_access,
    .rename = hfuse_rename,
    .open = hfuse_open,
    .release = hfuse_release,
    .write = hfuse_write,
    .read = hfuse_read,
    .init = hfuse_init,
    .destroy = hfuse_destroy,
    .releasedir = hfuse_releasedir,
    .chown = hfuse_chown,
    .chmod = hfuse_chmod,
    .utimens = hfuse_utimens,
    .truncate = hfuse_truncate,
    .flush = hfuse_flush,
    .fsync = hfuse_fsync,
    .unlink = hfuse_unlink,
    .rmdir = hfuse_rmdir,
    .statfs = hfuse_statfs,
 };

int hook_fuse(int argc, char **argv)
 {
  return fuse_main(argc,argv, &hfuse_ops, NULL);
 }