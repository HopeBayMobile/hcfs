#define FUSE_USE_VERSION 26
#include "fuseop.h"
#include "global.h"
#include "file_present.h"
#include "utils.h"
#include "dir_lookup.h"
#include "super_inode.h"
#include "params.h"
#include <fuse.h>
#include <time.h>
#include <math.h>
#include <sys/statvfs.h>
#include "hcfscurl.h"
#include "hcfs_tocloud.h"
#include "filetables.h"


/* TODO: Need to go over the access rights problem for the ops */
/* TODO: Access time may not be changed for file accesses, if noatime is specified in file opening or mounting. */
/*TODO: Need to revisit the error handling in all operations */
/*TODO: Will need to implement rollback or error marking when ops failed*/
/*TODO: Should consider using multiple FILE handler/pointer in one opened file. Could be used for multiple blocks or for a single block for multiple read ops*/
/*TODO: Should handle updating number of blocks and other uncovered info in an inode*/

/*TODO: A file-handle table manager that dynamically allocate extra block pointers and recycle them if not in use (but file not closed). Number of block pointers that can be allocated can be a variable of available process-wide opened files*/



static int hfuse_getattr(const char *path, struct stat *inode_stat)
 {
  ino_t hit_inode;
  int ret_code;

  hit_inode = lookup_pathname(path, &ret_code);

  if (hit_inode > 0)
   {
    ret_code = fetch_inode_stat(hit_inode, inode_stat);

    #if DEBUG >= 5
    printf("getattr %lld, returns %d\n",inode_stat->st_ino,ret_code);
    #endif  /* DEBUG */

    return ret_code;
   }
  else
   return ret_code;
 }

//int hfuse_readlink(const char *path, char *buf, size_t buf_size);

static int hfuse_mknod(const char *path, mode_t mode, dev_t dev)
 {
  char *parentname;
  char selfname[400];
  char thismetapath[METAPATHLEN];
  ino_t self_inode, parent_inode;
  struct stat this_stat;
  FILE_META_TYPE this_meta;
  mode_t self_mode;
  FILE *metafptr;
  int ret_val;
  struct fuse_context *temp_context;
  int ret_code;

  parentname = malloc(strlen(path)*sizeof(char));
  parse_parent_self(path,parentname,selfname);

  parent_inode = lookup_pathname(parentname, &ret_code);

  free(parentname);
  if (parent_inode < 1)
   return ret_code;

  memset(&this_stat,0,sizeof(struct stat));
  memset(&this_meta,0,sizeof(FILE_META_TYPE));
  temp_context = fuse_get_context();

  self_mode = mode | S_IFREG;
  this_stat.st_mode = self_mode;
  this_stat.st_size = 0;
  this_stat.st_blksize = MAX_BLOCK_SIZE;
  this_stat.st_blocks = 0;
  this_stat.st_dev = dev;
  this_stat.st_nlink = 1;
  this_stat.st_uid = temp_context->uid; /*Use the uid and gid of the fuse caller*/
  this_stat.st_gid = temp_context->gid;
  this_stat.st_atime = time(NULL);
  this_stat.st_mtime = this_stat.st_atime;
  this_stat.st_ctime = this_stat.st_ctime;

  self_inode = super_inode_new_inode(&this_stat);
  if (self_inode < 1)
   return -EACCES;
  this_stat.st_ino = self_inode;

  fetch_meta_path(thismetapath,self_inode);

  metafptr = fopen(thismetapath,"w");

  if (metafptr == NULL)
   return -EACCES;

  ret_val = fwrite(&this_stat,sizeof(struct stat),1,metafptr);
  if (ret_val < 1)
   {
    fclose(metafptr);
    unlink(thismetapath);
    return -EACCES;
   }

  ret_val = fwrite(&this_meta,sizeof(FILE_META_TYPE),1,metafptr);
  fclose(metafptr);
  if (ret_val < 1)
   {
    unlink(thismetapath);
    return -EACCES;
   }

  ret_val = dir_add_entry(parent_inode, self_inode, selfname,self_mode);
  if (ret_val < 0)
   {
    unlink(thismetapath);
    return -EACCES;
   }

  super_inode_mark_dirty(self_inode);

  return 0;
 }
static int hfuse_mkdir(const char *path, mode_t mode)
 {
  char *parentname;
  char selfname[400];
  char thismetapath[METAPATHLEN];
  ino_t self_inode, parent_inode;
  struct stat this_stat;
  DIR_META_TYPE this_meta;
  DIR_ENTRY_PAGE temppage;
  mode_t self_mode;
  FILE *metafptr;
  int ret_val;
  struct fuse_context *temp_context;
  int ret_code;

  parentname = malloc(strlen(path)*sizeof(char));
  parse_parent_self(path,parentname,selfname);

  parent_inode = lookup_pathname(parentname, &ret_code);

  free(parentname);
  if (parent_inode < 1)
   return ret_code;

  memset(&this_stat,0,sizeof(struct stat));
  memset(&this_meta,0,sizeof(DIR_META_TYPE));
  memset(&temppage,0,sizeof(DIR_ENTRY_PAGE));
  temp_context = fuse_get_context();

  self_mode = mode | S_IFDIR;
  this_stat.st_mode = self_mode;
  this_stat.st_nlink = 2;   /*One pointed by the parent, another by self*/
  this_stat.st_uid = temp_context->uid;   /*Use the uid and gid of the fuse caller*/
  this_stat.st_gid = temp_context->gid;

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

  fetch_meta_path(thismetapath,self_inode);

  metafptr = fopen(thismetapath,"w");

  if (metafptr == NULL)
   return -EACCES;


  ret_val = fwrite(&this_stat,sizeof(struct stat),1,metafptr);
  if (ret_val < 1)
   {
    fclose(metafptr);
    unlink(thismetapath);
    return -EACCES;
   }

  ret_val = fwrite(&this_meta,sizeof(DIR_META_TYPE),1,metafptr);
  if (ret_val < 1)
   {
    fclose(metafptr);
    unlink(thismetapath);
    return -EACCES;
   }

  this_meta.next_subdir_page = ftell(metafptr);
  fseek(metafptr,sizeof(struct stat),SEEK_SET);

  ret_val = fwrite(&this_meta,sizeof(DIR_META_TYPE),1,metafptr);

  if (ret_val < 1)
   {
    fclose(metafptr);
    unlink(thismetapath);
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
   {
    unlink(thismetapath);
    return -EACCES;
   }

  ret_val = dir_add_entry(parent_inode, self_inode, selfname,self_mode);
  if (ret_val < 0)
   {
    unlink(thismetapath);
    return -EACCES;
   }

  super_inode_mark_dirty(self_inode);

  return 0;
 }


int hfuse_unlink(const char *path)
 {
  char *parentname;
  char selfname[400];
  char thismetapath[METAPATHLEN];
  ino_t this_inode, parent_inode;
  int ret_val;
  int ret_code;

  this_inode = lookup_pathname(path, &ret_code);
  if (this_inode < 1)
   return ret_code;

  parentname = malloc(strlen(path)*sizeof(char));
  parse_parent_self(path,parentname,selfname);

  parent_inode = lookup_pathname(parentname, &ret_code);

  parse_parent_self(path,parentname,selfname);

  free(parentname);
  if (parent_inode < 1)
   return ret_code;

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
  char thismetapath[METAPATHLEN];
  char todelete_metapath[400];
  ino_t this_inode, parent_inode;
  int ret_val;
  FILE *metafptr;
  DIR_META_TYPE tempmeta;
  FILE *todeletefptr;
  char filebuf[5000];
  size_t read_size;
  int ret_code;

  this_inode = lookup_pathname(path, &ret_code);
  if (this_inode < 1)
   return ret_code;

  parentname = malloc(strlen(path)*sizeof(char));
  parse_parent_self(path,parentname,selfname);

  parse_parent_self(path,parentname,selfname);

  free(parentname);
  if (!strcmp(selfname,"."))
   return -EINVAL;
  if (!strcmp(selfname,".."))
   return -ENOTEMPTY;

  parent_inode = lookup_pathname(parentname, &ret_code);

  if (parent_inode < 1)
   return ret_code;

  invalidate_cache_entry(path);

  fetch_meta_path(thismetapath,this_inode);

  metafptr = fopen(thismetapath,"r+");

  if (metafptr == NULL)
   return -EACCES;
  setbuf(metafptr,NULL);
  flock(fileno(metafptr),LOCK_EX);
  fseek(metafptr,sizeof(struct stat),SEEK_SET);
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
  int ret_code, ret_code2;

  self_inode = lookup_pathname(oldpath, &ret_code);
  if (self_inode < 1)
   return ret_code;

  invalidate_cache_entry(oldpath);

  if (lookup_pathname(newpath, &ret_code) > 0)
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

  parent_inode1 = lookup_pathname(parentname1, &ret_code);

  parent_inode2 = lookup_pathname(parentname2, &ret_code2);

  free(parentname1);
  free(parentname2);

  if (parent_inode1 < 1)
   return ret_code;

  if (parent_inode2 < 1)
   return ret_code2;

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
  struct stat temp_inode_stat;
  int ret_val;
  ino_t this_inode;
  char thismetapath[METAPATHLEN];
  FILE *fptr;
  int ret_code;

  printf("Debug chmod\n");
  this_inode = lookup_pathname(path, &ret_code);
  if (this_inode < 1)
   return ret_code;

  fetch_meta_path(thismetapath,this_inode);
  printf("%lld %s\n",this_inode,thismetapath);
  fptr = fopen(thismetapath,"r+");
  if (fptr==NULL)
   return -ENOENT;
  setbuf(fptr,NULL);
  
  flock(fileno(fptr),LOCK_EX);
  fread(&temp_inode_stat,sizeof(struct stat),1,fptr);
  temp_inode_stat.st_mode = mode;
  temp_inode_stat.st_ctime = time(NULL);
  fseek(fptr,0,SEEK_SET);
  fwrite(&temp_inode_stat,sizeof(struct stat),1,fptr);
  memcpy(&(tempentry.inode_stat),&temp_inode_stat,sizeof(struct stat));
  flock(fileno(fptr),LOCK_UN);
  fclose(fptr);
  super_inode_write(this_inode, &tempentry);

  return 0;
 }

int hfuse_chown(const char *path, uid_t owner, gid_t group)
 {
  SUPER_INODE_ENTRY tempentry;
  struct stat temp_inode_stat;
  int ret_val;
  ino_t this_inode;
  char thismetapath[METAPATHLEN];
  FILE *fptr;
  int ret_code;

  printf("Debug chmod\n");
  this_inode = lookup_pathname(path, &ret_code);
  if (this_inode < 1)
   return ret_code;

  fetch_meta_path(thismetapath,this_inode);
  printf("%lld %s\n",this_inode,thismetapath);
  fptr = fopen(thismetapath,"r+");
  if (fptr==NULL)
   return -ENOENT;
  setbuf(fptr,NULL);
  
  flock(fileno(fptr),LOCK_EX);
  fread(&temp_inode_stat,sizeof(struct stat),1,fptr);
  temp_inode_stat.st_uid = owner;
  temp_inode_stat.st_gid = group;
  temp_inode_stat.st_ctime = time(NULL);
  fseek(fptr,0,SEEK_SET);
  fwrite(&temp_inode_stat,sizeof(struct stat),1,fptr);
  memcpy(&(tempentry.inode_stat),&temp_inode_stat,sizeof(struct stat));
  flock(fileno(fptr),LOCK_UN);
  fclose(fptr);
  super_inode_write(this_inode, &tempentry);

  return 0;
 }


int hfuse_truncate(const char *path, off_t offset)
 {
/*TODO: If truncate file smaller, do not truncate metafile, but instead set the affected entries to ST_TODELETE (which will be changed to ST_NONE once object deleted)*/
/*TODO: Add ST_TODELETE as a new block status. In truncate, if need to throw away a block, set the status to ST_TODELETE and upload process will handle the actual deletion.*/
/*If need to truncate some block that's ST_CtoL or ST_CLOUD, download it first, mod it, then set to ST_LDISK*/


  SUPER_INODE_ENTRY tempentry;
  struct stat tempfilestat;
  FILE_META_TYPE tempfilemeta;
  int ret_val;
  ino_t this_inode;
  char thismetapath[METAPATHLEN];
  char thisblockpath[1024];
  FILE *fptr,*blockfptr;
  long long last_block,last_page, old_last_block;
  long long current_page;
  off_t nextfilepos, prevfilepos, currentfilepos;
  BLOCK_ENTRY_PAGE temppage;
  int last_entry_index;
  off_t old_block_size,new_block_size;
  int block_count;
  long long temp_block_index;
  struct stat tempstat;
  int ret_code;

  this_inode = lookup_pathname(path, &ret_code);
  if (this_inode < 1)
   return ret_code;

  fetch_meta_path(thismetapath,this_inode);
  fptr = fopen(thismetapath,"r+");
  if (fptr==NULL)
   return -ENOENT;
  setbuf(fptr,NULL);
  
  super_inode_read(this_inode, &tempentry);

  flock(fileno(fptr),LOCK_EX);
  if (tempentry.inode_stat.st_mode & S_IFREG)
   {
    fread(&tempfilestat,sizeof(struct stat),1,fptr);
    fread(&tempfilemeta,sizeof(FILE_META_TYPE),1,fptr);
    if (tempfilestat.st_size == offset)
     {
      /*Do nothing if no change needed */
      printf("Debug truncate: no size change. Nothing changed.\n");
      flock(fileno(fptr),LOCK_UN);
      fclose(fptr);
      return 0;
     }

    if (tempfilestat.st_size < offset)
     {
      /*If need to extend, only need to change st_size*/

      sem_wait(&(hcfs_system->access_sem));
      hcfs_system->systemdata.system_size += (long long)(offset - tempfilestat.st_size);
      sync_hcfs_system_data(FALSE);
      sem_post(&(hcfs_system->access_sem));           

      tempfilestat.st_size = offset;
     }
    else
     {
      if (offset == 0)
       {
        last_block = -1;
        last_page = -1;
       }
      else
       {
        last_block = ((offset-1) / MAX_BLOCK_SIZE);  /* Block indexing starts at zero */

        last_page = last_block / MAX_BLOCK_ENTRIES_PER_PAGE; /*Page indexing starts at zero*/
       }

      old_last_block = ((tempfilestat.st_size - 1) / MAX_BLOCK_SIZE);
      nextfilepos = tempfilemeta.next_block_page;

      current_page = 0;
      prevfilepos = 0;

      temp_block_index = last_block+1;

      /*TODO: put error handling for the read/write ops here*/
      while(current_page <= last_page)
       {
        if (nextfilepos == 0) /*Data after offset does not actually exists. Just change file size */
         {
          sem_wait(&(hcfs_system->access_sem));
          hcfs_system->systemdata.system_size += (long long)(offset - tempfilestat.st_size);
          sync_hcfs_system_data(FALSE);
          sem_post(&(hcfs_system->access_sem));
          tempfilestat.st_size = offset;
          break;
         }
        else
         {
          fseek(fptr, nextfilepos, SEEK_SET);
          prevfilepos = nextfilepos;
          fread(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
          nextfilepos = temppage.next_page;
         }
        if (current_page == last_page)
         {
          /* TODO: Do the actual handling here*/
          currentfilepos = prevfilepos;
          last_entry_index = last_block % MAX_BLOCK_ENTRIES_PER_PAGE;
          if ((offset % MAX_BLOCK_SIZE) != 0)
           {
            /*Offset not on the boundary of the block. Will need to truncate the last block*/
            while (((temppage).block_entries[last_entry_index].status == ST_CLOUD) ||
                   ((temppage).block_entries[last_entry_index].status == ST_CtoL))
             {
              if (hcfs_system->systemdata.cache_size > CACHE_HARD_LIMIT) /*Sleep if cache already full*/
               {
                printf("debug truncate waiting on full cache\n");
                flock(fileno(fptr),LOCK_UN);
                sleep_on_cache_full();

              /*Re-read status*/
                flock(fileno(fptr),LOCK_EX);
                fseek(fptr,0,SEEK_SET);
                fread(&tempfilestat,sizeof(struct stat),1,fptr);
                fread(&tempfilemeta,sizeof(FILE_META_TYPE),1,fptr);
                fseek(fptr,currentfilepos,SEEK_SET);
                fread(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
               }
              else
               break;
             }

            fetch_block_path(thisblockpath,tempfilestat.st_ino,last_block);

            if (((temppage).block_entries[last_entry_index].status == ST_CLOUD) ||
                   ((temppage).block_entries[last_entry_index].status == ST_CtoL))
             {
              /*Download from backend */
              blockfptr = fopen(thisblockpath,"a+");
              fclose(blockfptr);
              blockfptr = fopen(thisblockpath,"r+");
              setbuf(blockfptr,NULL);
              flock(fileno(blockfptr),LOCK_EX);
              fseek(fptr,currentfilepos,SEEK_SET);
              fread(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
              if (((temppage).block_entries[last_entry_index].status == ST_CLOUD) ||
                  ((temppage).block_entries[last_entry_index].status == ST_CtoL))
               {
                if ((temppage).block_entries[last_entry_index].status == ST_CLOUD)
                 {
                  (temppage).block_entries[last_entry_index].status = ST_CtoL;
                  fseek(fptr,currentfilepos,SEEK_SET);
                  fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
                  fflush(fptr);
                 }
                flock(fileno(fptr),LOCK_UN);
                fetch_from_cloud(blockfptr,tempfilestat.st_ino,last_block);

                /*Re-read status*/
                flock(fileno(fptr),LOCK_EX);
                fseek(fptr,currentfilepos,SEEK_SET);
                fread(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);

                if (stat(thisblockpath,&tempstat)==0)
                 {
                  (temppage).block_entries[last_entry_index].status = ST_LDISK;
                  setxattr(thisblockpath,"user.dirty","T",1,0);
                  fseek(fptr,currentfilepos,SEEK_SET);
                  fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
                  fflush(fptr);

                  sem_wait(&(hcfs_system->access_sem));
                  hcfs_system->systemdata.cache_size += tempstat.st_size;
                  hcfs_system->systemdata.cache_blocks++;
                  sync_hcfs_system_data(FALSE);
                  sem_post(&(hcfs_system->access_sem));           
                 }
               }
              else
               {
                if (stat(thisblockpath,&tempstat)==0)
                 {
                  (temppage).block_entries[last_entry_index].status = ST_LDISK;
                  setxattr(thisblockpath,"user.dirty","T",1,0);
                  fseek(fptr,currentfilepos,SEEK_SET);
                  fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
                  fflush(fptr);
                 }
               }
              old_block_size = check_file_size(thisblockpath);
              ftruncate(fileno(blockfptr),(offset % MAX_BLOCK_SIZE));
              new_block_size = check_file_size(thisblockpath);

              sem_wait(&(hcfs_system->access_sem));
              hcfs_system->systemdata.cache_size += new_block_size - old_block_size;
              hcfs_system->systemdata.cache_blocks++;
              sync_hcfs_system_data(FALSE);
              sem_post(&(hcfs_system->access_sem));           

              flock(fileno(blockfptr),LOCK_UN);
              fclose(blockfptr);
             }
            else
             {
              blockfptr = fopen(thisblockpath,"r+");
              setbuf(blockfptr,NULL);
              flock(fileno(blockfptr),LOCK_EX);

              if (stat(thisblockpath,&tempstat)==0)
               {
                (temppage).block_entries[last_entry_index].status = ST_LDISK;
                setxattr(thisblockpath,"user.dirty","T",1,0);
                fseek(fptr,currentfilepos,SEEK_SET);
                fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
                fflush(fptr);
               }

              old_block_size = check_file_size(thisblockpath);
              ftruncate(fileno(blockfptr),(offset % MAX_BLOCK_SIZE));
              new_block_size = check_file_size(thisblockpath);

              sem_wait(&(hcfs_system->access_sem));
              hcfs_system->systemdata.cache_size += new_block_size - old_block_size;
              hcfs_system->systemdata.cache_blocks++;
              sync_hcfs_system_data(FALSE);
              sem_post(&(hcfs_system->access_sem));           

              flock(fileno(blockfptr),LOCK_UN);
              fclose(blockfptr);
             }
           }

          /*Clean up the rest of blocks in this same page as well*/
          for (block_count = last_entry_index + 1; block_count < MAX_BLOCK_ENTRIES_PER_PAGE; block_count ++)
           {
            if (temp_block_index > old_last_block)
             break;
            switch ((temppage).block_entries[block_count].status)
             {
              case ST_NONE: 
              case ST_TODELETE:
                  break;
              case ST_LDISK:
                  fetch_block_path(thisblockpath,tempfilestat.st_ino,temp_block_index);
                  unlink(thisblockpath);
                  (temppage).block_entries[block_count].status = ST_NONE;
                  break;
              case ST_CLOUD:
                  (temppage).block_entries[block_count].status = ST_TODELETE;
                  break;
              case ST_BOTH:
              case ST_LtoC:
              case ST_CtoL:
                  fetch_block_path(thisblockpath,tempfilestat.st_ino,temp_block_index);
                  if (access(thisblockpath,F_OK)==0)
                   unlink(thisblockpath);
                  (temppage).block_entries[block_count].status = ST_TODELETE;
                  break;
              default:
                  break;
             }
            temp_block_index++;
           }
          fseek(fptr,currentfilepos,SEEK_SET);
          fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
          fflush(fptr);

          sem_wait(&(hcfs_system->access_sem));
          hcfs_system->systemdata.system_size += (long long)(offset - tempfilestat.st_size);
          sync_hcfs_system_data(FALSE);
          sem_post(&(hcfs_system->access_sem));   
          tempfilestat.st_size = offset;
          break;
         }
        else
         current_page++;
       }
      /*Clean up the rest of the block status pages if any*/

      while(nextfilepos != 0)
       {
        currentfilepos = nextfilepos;
        fseek(fptr,currentfilepos,SEEK_SET);
        fread(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
        nextfilepos = temppage.next_page;
        for (block_count = 0; block_count < MAX_BLOCK_ENTRIES_PER_PAGE; block_count ++)
         {
          if (temp_block_index > old_last_block)
           break;
          switch ((temppage).block_entries[block_count].status)
           {
            case ST_NONE: 
            case ST_TODELETE:
                break;
            case ST_LDISK:
                fetch_block_path(thisblockpath,tempfilestat.st_ino,temp_block_index);
                unlink(thisblockpath);
                (temppage).block_entries[block_count].status = ST_NONE;
                break;
            case ST_CLOUD:
                (temppage).block_entries[block_count].status = ST_TODELETE;
                break;
            case ST_BOTH:
            case ST_LtoC:
            case ST_CtoL:
                fetch_block_path(thisblockpath,tempfilestat.st_ino,temp_block_index);
                if (access(thisblockpath,F_OK)==0)
                 unlink(thisblockpath);
                (temppage).block_entries[block_count].status = ST_TODELETE;
                break;
            default:
                break;
           }
          temp_block_index++;
         }
        fseek(fptr,currentfilepos,SEEK_SET);
        fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
        fflush(fptr);
       }

     }


    tempfilestat.st_mtime = time(NULL);
    fseek(fptr,0,SEEK_SET);
    fwrite(&tempfilestat,sizeof(struct stat),1,fptr);
    fwrite(&tempfilemeta,sizeof(FILE_META_TYPE),1,fptr);
    memcpy(&(tempentry.inode_stat),&(tempfilestat),sizeof(struct stat));
    flock(fileno(fptr),LOCK_UN);
    fclose(fptr);
    super_inode_write(this_inode, &tempentry);
   }  
  else
   {
    if (tempentry.inode_stat.st_mode & S_IFDIR)
     {
      flock(fileno(fptr),LOCK_UN);
      fclose(fptr);
      return -EISDIR;
     } 
    else
     {
      flock(fileno(fptr),LOCK_UN);
      fclose(fptr);
      return -EACCES;
     }
   }

  return 0;
 }
int hfuse_open(const char *path, struct fuse_file_info *file_info)
 {
  /*TODO: Need to check permission here*/
  ino_t thisinode;
  long long fh;
  int ret_code;

  thisinode = lookup_pathname(path, &ret_code);
  if (thisinode < 1)
   return ret_code;

  fh = open_fh(thisinode);
  if (fh < 0)
   return -ENFILE;

  file_info->fh = fh;

  return 0;
 }

int hfuse_read(const char *path, char *buf, size_t size_org, off_t offset, struct fuse_file_info *file_info)
 {
  FH_ENTRY *fh_ptr;
  long long start_block, end_block, current_block;
  long long start_page, end_page, current_page;
  off_t nextfilepos, prevfilepos, currentfilepos;
  BLOCK_ENTRY_PAGE temppage;
  long long entry_index;
  long long block_index;
  char thisblockpath[400];
  int total_bytes_read;
  int this_bytes_read;
  off_t current_offset;
  int target_bytes_read;
  size_t size;
  char fill_zeros;
  struct stat tempstat;
  PREFETCH_STRUCT_TYPE *temp_prefetch;
  pthread_t prefetch_thread;
  off_t this_page_fpos;


/*TODO: Perhaps should do proof-checking on the inode number using pathname lookup and from file_info*/
/*TODO: Fixme: cached page pointer and file offset may be changed during the operation, so cannot be used as a reliable source for seeking and read/write meta */
  if (system_fh_table.entry_table_flags[file_info->fh] == FALSE)
   return 0;

  fh_ptr = &(system_fh_table.entry_table[file_info->fh]);

  flockfile(fh_ptr-> metafptr);
  fseek(fh_ptr->metafptr,0,SEEK_SET);
  fread(&(fh_ptr->cached_stat),sizeof(struct stat),1,fh_ptr->metafptr);
  funlockfile(fh_ptr-> metafptr);


  if ((fh_ptr->cached_stat).st_size < (offset+size_org))
   size = ((fh_ptr->cached_stat).st_size - offset);
  else
   size = size_org;

  if (size <=0)
   return 0;

  total_bytes_read = 0;

  start_block = (offset / MAX_BLOCK_SIZE);  /* Block indexing starts at zero */
  end_block = ((offset+size-1) / MAX_BLOCK_SIZE);

  start_page = start_block / MAX_BLOCK_ENTRIES_PER_PAGE; /*Page indexing starts at zero*/
  end_page = end_block / MAX_BLOCK_ENTRIES_PER_PAGE;


  flockfile(fh_ptr-> metafptr);
  if (fh_ptr->cached_page_index != start_page)
   {
    flock(fileno(fh_ptr-> metafptr),LOCK_EX);
    fseek(fh_ptr->metafptr,0,SEEK_SET);
    fread(&(fh_ptr->cached_stat),sizeof(struct stat),1,fh_ptr->metafptr);

    if (fh_ptr->cached_page_index != start_page)  /*Check if other threads have already done the work*/
     seek_page(fh_ptr-> metafptr,fh_ptr, start_page);
    flock(fileno(fh_ptr->metafptr),LOCK_UN);
   }
  this_page_fpos = fh_ptr->cached_page_start_fpos;
  funlockfile(fh_ptr->metafptr);

  entry_index = start_block % MAX_BLOCK_ENTRIES_PER_PAGE;

  for(block_index = start_block; block_index <= end_block; block_index++)
   {
    sem_wait(&(fh_ptr->block_sem));
    fill_zeros = FALSE;
    while (fh_ptr->opened_block != block_index)
     {
      if (fh_ptr->opened_block != -1)
       {
        fclose(fh_ptr->blockfptr);
        fh_ptr->opened_block = -1;
       }

      flockfile(fh_ptr->metafptr);
      fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
      fread(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
      funlockfile(fh_ptr->metafptr);

      while (((temppage).block_entries[entry_index].status == ST_CLOUD) ||
             ((temppage).block_entries[entry_index].status == ST_CtoL))
       {
        if (hcfs_system->systemdata.cache_size > CACHE_HARD_LIMIT) /*Sleep if cache already full*/
         {
          sem_post(&(fh_ptr->block_sem));
          printf("debug read waiting on full cache\n");
          sleep_on_cache_full();
          sem_wait(&(fh_ptr->block_sem));
          /*Re-read status*/
          flockfile(fh_ptr->metafptr);
          fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
          fread(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
          funlockfile(fh_ptr->metafptr);
         }
        else
         break;
       }

      if ((entry_index+1) < MAX_BLOCK_ENTRIES_PER_PAGE)
       {
        if (((temppage).block_entries[entry_index+1].status == ST_CLOUD) || 
            ((temppage).block_entries[entry_index+1].status == ST_CtoL))
         {
          temp_prefetch = malloc(sizeof(PREFETCH_STRUCT_TYPE));
          temp_prefetch -> this_inode = (fh_ptr->cached_stat).st_ino;
          temp_prefetch -> block_no = block_index + 1;
          temp_prefetch -> page_start_fpos = this_page_fpos;
          temp_prefetch -> entry_index = entry_index + 1;
          pthread_create(&(prefetch_thread),&prefetch_thread_attr,(void *)&prefetch_block, ((void *)temp_prefetch));
         }
       }

      switch((temppage).block_entries[entry_index].status)
       {
        case ST_NONE: 
        case ST_TODELETE:
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
            fetch_block_path(thisblockpath,(fh_ptr->cached_stat).st_ino,block_index);
            fh_ptr->blockfptr = fopen(thisblockpath,"a+");
            fclose(fh_ptr->blockfptr);
            fh_ptr->blockfptr = fopen(thisblockpath,"r+");
            setbuf(fh_ptr->blockfptr,NULL);
            flock(fileno(fh_ptr->blockfptr),LOCK_EX);
            flockfile(fh_ptr->metafptr);
            flock(fileno(fh_ptr->metafptr),LOCK_EX);
            fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
            fread(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
            if (((temppage).block_entries[entry_index].status == ST_CLOUD) ||
                ((temppage).block_entries[entry_index].status == ST_CtoL))
             {
              if ((temppage).block_entries[entry_index].status == ST_CLOUD)
               {
                (temppage).block_entries[entry_index].status = ST_CtoL;
                fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
                fwrite(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
                fflush(fh_ptr->metafptr);
               }
              flock(fileno(fh_ptr->metafptr),LOCK_UN);
              fetch_from_cloud(fh_ptr->blockfptr,(fh_ptr->cached_stat).st_ino,block_index);
              /*Do not process cache update and stored_where change if block is actually deleted by other ops such as truncate*/
              flock(fileno(fh_ptr->metafptr),LOCK_EX);
              fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
              fread(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
              if (stat(thisblockpath,&tempstat)==0)
               {
                (temppage).block_entries[entry_index].status = ST_BOTH;
                fsetxattr(fileno(fh_ptr->blockfptr),"user.dirty","F",1,0);
                fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
                fwrite(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
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
            setbuf(fh_ptr->blockfptr,NULL);
            fh_ptr->opened_block = block_index;
            
//            flock(fileno(fh_ptr->blockfptr),LOCK_UN);
//            fclose(fh_ptr->blockfptr);
            fill_zeros = FALSE;
            break;
        default:
            break;
       }

      if ((fill_zeros != TRUE) && (fh_ptr->opened_block != block_index))
       {
        fetch_block_path(thisblockpath,(fh_ptr->cached_stat).st_ino,block_index);

        fh_ptr->blockfptr=fopen(thisblockpath,"r+");
        if (fh_ptr->blockfptr != NULL)
         {
          setbuf(fh_ptr->blockfptr,NULL);
          fh_ptr->opened_block = block_index;
         }
        else  /*Some exception that block file is deleted in the middle of the status check*/
         {
          printf("Debug read: cannot open block file. Perhaps replaced?\n");
          fh_ptr->opened_block = -1;
         }
       }
      else
       break;
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
        printf("Short reading? %ld %d\n",current_offset, total_bytes_read + this_bytes_read);
        memset(&buf[total_bytes_read + this_bytes_read],0,sizeof(char) * (target_bytes_read - this_bytes_read));
        this_bytes_read = target_bytes_read;
       }
     }
    else
     {
      printf("Padding zeros? %ld %d\n",current_offset, total_bytes_read);
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
        this_page_fpos = advance_block(fh_ptr-> metafptr,this_page_fpos,&entry_index);
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
    fread(&(fh_ptr->cached_stat),sizeof(struct stat),1,fh_ptr->metafptr);


    if (total_bytes_read > 0)
     (fh_ptr->cached_stat).st_atime = time(NULL);

    fseek(fh_ptr->metafptr,0,SEEK_SET);
    fwrite(&(fh_ptr->cached_stat), sizeof(struct stat),1,fh_ptr->metafptr);

    super_inode_update_stat((fh_ptr->cached_stat).st_ino, &((fh_ptr->cached_stat)));

    flock(fileno(fh_ptr-> metafptr),LOCK_UN);
    funlockfile(fh_ptr-> metafptr);
   }

  return total_bytes_read;
 }

int hfuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *file_info)
 {
  FH_ENTRY *fh_ptr;
  long long start_block, end_block, current_block;
  long long start_page, end_page, current_page;
  off_t nextfilepos, prevfilepos, currentfilepos;
  BLOCK_ENTRY_PAGE temppage;
  long long entry_index;
  long long block_index;
  char thisblockpath[400];
  size_t total_bytes_written;
  size_t this_bytes_written;
  off_t current_offset;
  size_t target_bytes_written;
  off_t old_cache_size, new_cache_size;
  struct stat tempstat;
  off_t this_page_fpos;

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
  fread(&(fh_ptr->cached_stat),sizeof(struct stat),1,fh_ptr->metafptr);


  if (fh_ptr->cached_page_index != start_page)
   seek_page(fh_ptr-> metafptr,fh_ptr, start_page);

  this_page_fpos = fh_ptr->cached_page_start_fpos;

  entry_index = start_block % MAX_BLOCK_ENTRIES_PER_PAGE;

  for(block_index = start_block; block_index <= end_block; block_index++)
   {
    fetch_block_path(thisblockpath,(fh_ptr->cached_stat).st_ino,block_index);
    sem_wait(&(fh_ptr->block_sem));
    if (fh_ptr->opened_block != block_index)
     {
      if (fh_ptr->opened_block != -1)
       {
        fclose(fh_ptr->blockfptr);
        fh_ptr->opened_block = -1;
       }
      fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
      fread(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);

      while (((temppage).block_entries[entry_index].status == ST_CLOUD) ||
             ((temppage).block_entries[entry_index].status == ST_CtoL))
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
          fread(&(fh_ptr->cached_stat),sizeof(struct stat),1,fh_ptr->metafptr);
          fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
          fread(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
         }
        else
         break;
       }


      switch((temppage).block_entries[entry_index].status)
       {
        case ST_NONE:
        case ST_TODELETE:
             /*If not stored anywhere, make it on local disk*/
            fh_ptr->blockfptr=fopen(thisblockpath,"a+");
            fclose(fh_ptr->blockfptr);
            (temppage).block_entries[entry_index].status = ST_LDISK;
            setxattr(thisblockpath,"user.dirty","T",1,0);
            fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
            fwrite(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
            sem_wait(&(hcfs_system->access_sem));
            hcfs_system->systemdata.cache_blocks++;
            sync_hcfs_system_data(FALSE);
            sem_post(&(hcfs_system->access_sem));           
            break;
        case ST_LDISK:
            break;
        case ST_BOTH:
        case ST_LtoC:
            (temppage).block_entries[entry_index].status = ST_LDISK;
            setxattr(thisblockpath,"user.dirty","T",1,0);
            fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
            fwrite(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
            break;
        case ST_CLOUD:
        case ST_CtoL:        
            /*Download from backend */
            fetch_block_path(thisblockpath,(fh_ptr->cached_stat).st_ino,block_index);
            fh_ptr->blockfptr = fopen(thisblockpath,"a+");
            fclose(fh_ptr->blockfptr);
            fh_ptr->blockfptr = fopen(thisblockpath,"r+");
            setbuf(fh_ptr->blockfptr,NULL);
            flock(fileno(fh_ptr->blockfptr),LOCK_EX);
            fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
            fread(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
            if (((temppage).block_entries[entry_index].status == ST_CLOUD) ||
                ((temppage).block_entries[entry_index].status == ST_CtoL))
             {
              if ((temppage).block_entries[entry_index].status == ST_CLOUD)
               {
                (temppage).block_entries[entry_index].status = ST_CtoL;
                fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
                fwrite(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
                fflush(fh_ptr->metafptr);
               }
              flock(fileno(fh_ptr-> metafptr),LOCK_UN);
              fetch_from_cloud(fh_ptr->blockfptr,(fh_ptr->cached_stat).st_ino,block_index);
              /*Do not process cache update and stored_where change if block is actually deleted by other ops such as truncate*/

              /*Re-read status*/
              flock(fileno(fh_ptr-> metafptr),LOCK_EX);
              fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
              fread(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);

              if (stat(thisblockpath,&tempstat)==0)
               {
                (temppage).block_entries[entry_index].status = ST_LDISK;
                setxattr(thisblockpath,"user.dirty","T",1,0);
                fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
                fwrite(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
                fflush(fh_ptr->metafptr);

                sem_wait(&(hcfs_system->access_sem));
                hcfs_system->systemdata.cache_size += tempstat.st_size;
                hcfs_system->systemdata.cache_blocks++;
                sync_hcfs_system_data(FALSE);
                sem_post(&(hcfs_system->access_sem));           
               }
             }
            else
             {
              if (stat(thisblockpath,&tempstat)==0)
               {
                (temppage).block_entries[entry_index].status = ST_LDISK;
                setxattr(thisblockpath,"user.dirty","T",1,0);
                fseek(fh_ptr->metafptr, this_page_fpos,SEEK_SET);
                fwrite(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
                fflush(fh_ptr->metafptr);
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
     this_page_fpos=advance_block(fh_ptr-> metafptr,this_page_fpos,&entry_index);
   }

  /*Update and flush file meta*/

  fseek(fh_ptr->metafptr,0,SEEK_SET);
  fread(&(fh_ptr->cached_stat),sizeof(struct stat),1,fh_ptr->metafptr);

  if ((fh_ptr->cached_stat).st_size < (offset + total_bytes_written))
   {
    sem_wait(&(hcfs_system->access_sem));
    hcfs_system->systemdata.system_size += (long long) ((offset + total_bytes_written) - (fh_ptr->cached_stat).st_size);
    sync_hcfs_system_data(FALSE);
    sem_post(&(hcfs_system->access_sem));           

    (fh_ptr->cached_stat).st_size = (offset + total_bytes_written);
    (fh_ptr->cached_stat).st_blocks = ((fh_ptr->cached_stat).st_size +511) / 512;
   }

  if (total_bytes_written > 0)
   (fh_ptr->cached_stat).st_mtime = time(NULL);

  fseek(fh_ptr->metafptr,0,SEEK_SET);
  fwrite(&(fh_ptr->cached_stat), sizeof(struct stat),1,fh_ptr->metafptr);

  super_inode_update_stat((fh_ptr->cached_stat).st_ino, &((fh_ptr->cached_stat)));

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
   buf->f_blocks = (25*powl(1024,2));

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
  int ret_code;

  thisinode = lookup_pathname(path, &ret_code);
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
  off_t thisfile_pos;
  DIR_META_TYPE tempmeta;
  DIR_ENTRY_PAGE temp_page;
  struct stat tempstat;
  int ret_code;

/*TODO: Need to include symlinks*/
/*TODO: Will need to test the boundary of the operation. When will buf run out of space?*/
  fprintf(stderr,"DEBUG readdir entering readdir\n");

  this_inode = lookup_pathname(path, &ret_code);

  if (this_inode == 0)
   return ret_code;

  fetch_meta_path(pathname,this_inode);
  fptr = fopen(pathname,"r");
  setbuf(fptr,NULL);
  flock(fileno(fptr),LOCK_SH);

  fseek(fptr,sizeof(struct stat),SEEK_SET);
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
  pthread_attr_init(&prefetch_thread_attr);
  pthread_attr_setdetachstate(&prefetch_thread_attr,PTHREAD_CREATE_DETACHED);
  return ((void*) sys_super_inode);
 }
void hfuse_destroy(void *private_data)
 {
  int download_handle_count;

  for(download_handle_count=0;download_handle_count<MAX_DOWNLOAD_CURL_HANDLE;download_handle_count++)
   {
    hcfs_destroy_backend(download_curl_handles[download_handle_count].curl);
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
  struct stat temp_inode_stat;
  int ret_val;
  ino_t this_inode;
  char thismetapath[METAPATHLEN];
  FILE *fptr;
  int ret_code;

  printf("Debug chmod\n");
  this_inode = lookup_pathname(path, &ret_code);
  if (this_inode < 1)
   return ret_code;

  fetch_meta_path(thismetapath,this_inode);
  printf("%lld %s\n",this_inode,thismetapath);
  fptr = fopen(thismetapath,"r+");
  if (fptr==NULL)
   return -ENOENT;
  setbuf(fptr,NULL);
  
  flock(fileno(fptr),LOCK_EX);
  fread(&temp_inode_stat,sizeof(struct stat),1,fptr);
  temp_inode_stat.st_atime = (time_t)(tv[0].tv_sec);
  temp_inode_stat.st_mtime = (time_t)(tv[1].tv_sec);
  fseek(fptr,0,SEEK_SET);
  fwrite(&temp_inode_stat,sizeof(struct stat),1,fptr);
  memcpy(&(tempentry.inode_stat),&temp_inode_stat,sizeof(struct stat));
  flock(fileno(fptr),LOCK_UN);
  fclose(fptr);
  super_inode_write(this_inode, &tempentry);

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
