#include "fuseop.h"
#include "global.h"
#include "dir_lookup.h"
#include "params.h"
/*TODO: need to modify pathname lookup to handle symlink*/

void init_pathname_cache()
 {
  long long count;

  for(count=0;count<PATHNAME_CACHE_ENTRY_NUM;count++)
   {
    memset(&(pathname_cache[count]),0,sizeof(PATHNAME_CACHE_ENTRY));
    sem_init(&(pathname_cache[count].cache_entry_sem),0,1);
   }

  return;
 }

unsigned long long compute_hash(const char *path)
 {
  unsigned long long seed=0;
  long long count;
  unsigned char temp;
  unsigned long long temp1;
  unsigned long long mode_base;

  if (PATHNAME_CACHE_ENTRY_NUM > 65536)
   mode_base = PATHNAME_CACHE_ENTRY_NUM;
  else
   mode_base = 65536;

  for(count=0;count<strlen(path);count++)
   {
    temp = (unsigned char) path[count];
    temp1 = (unsigned long long) temp;
    seed = (3 * seed + (temp1 * 13)) % mode_base;
   }
  seed = seed % PATHNAME_CACHE_ENTRY_NUM;

  return seed;
 }

void replace_pathname_cache(long long index, char *path, ino_t inode_number)
 {
  if (strlen(path) > MAX_PATHNAME)
   return;

  sem_wait(&(pathname_cache[index].cache_entry_sem));
  strcpy(pathname_cache[index].pathname,path);
  pathname_cache[index].inode_number = inode_number;
  sem_post(&(pathname_cache[index].cache_entry_sem));

  return;
 }
void invalidate_cache_entry(const char *path)
 {
  unsigned long long index;

  if (strlen(path) > MAX_PATHNAME)
   return;

  index = compute_hash(path);
  sem_wait(&(pathname_cache[index].cache_entry_sem));
  if (strcmp(pathname_cache[index].pathname,path)==0)
   {
    /*Occupant is this path name */
    pathname_cache[index].pathname[0] = 0;
    pathname_cache[index].inode_number = 0;
   }
  sem_post(&(pathname_cache[index].cache_entry_sem));
  return;
 }

ino_t check_cached_path(const char *path)
 {
  unsigned long long index;
  ino_t return_val;

  if (strlen(path) > MAX_PATHNAME)
   return 0;

  index = compute_hash(path);
  sem_wait(&(pathname_cache[index].cache_entry_sem));
  if (strcmp(pathname_cache[index].pathname,path)!=0)
   return_val = 0;
  else
   return_val = pathname_cache[index].inode_number;
  sem_post(&(pathname_cache[index].cache_entry_sem));
  return return_val;
 }

/* TODO: resolving pathname may result in EACCES */

ino_t lookup_pathname(const char *path, int *errcode)
 {
  int strptr;
  unsigned long long index;
  char tempdir[MAX_PATHNAME+10];
  ino_t cached_inode;

  *errcode = -ENOENT;

  if (strcmp(path,"/")==0)  /*Root of the FUSE system has inode 1. */
   return 1;

  strptr = strlen(path);
  while(strptr > 1)
   {
    if (strptr > MAX_PATHNAME)
     strptr--;
    else
     {
      if (strptr == strlen(path))
       {
        cached_inode = check_cached_path(path);
        if (cached_inode > 0)
         return cached_inode;
        strptr--;
       }
      else
       {
        if (path[strptr-1]!='/')
         strptr--;
        else
         {
          strncpy(tempdir,path,strptr);
          tempdir[strptr-1]=0;
          cached_inode = check_cached_path(tempdir);
          if (cached_inode > 0)
           return lookup_pathname_recursive(cached_inode, strptr-1, &path[strptr-1],path, errcode);
          strptr--;
         }
       }
     }
   }
  return lookup_pathname_recursive(1, 0, path,path, errcode);
 }
ino_t lookup_pathname_recursive(ino_t subroot, int prepath_length, const char *partialpath, const char *fullpath, int *errcode)
 {
  FILE *fptr;
  int count;
  int new_prepath_length;
  char search_subdir_only;
  char metapathname[METAPATHLEN];
  char target_entry_name[400];
  char tempname[400];
  off_t thisfile_pos;
  ino_t hit_inode;
  DIR_META_TYPE tempmeta;
  DIR_ENTRY_PAGE temp_page;
  int ret_val;

  if ((partialpath[0]=='/') && (strlen(partialpath)==1))
   return subroot;

  search_subdir_only = FALSE;
  for(count=1;count<strlen(partialpath);count++)
   if (partialpath[count]=='/')
    {
     search_subdir_only = TRUE;
     strncpy(target_entry_name,&(partialpath[1]),count-1);
     target_entry_name[count-1]=0;
     break;
    }

  fetch_meta_path(metapathname, subroot);

  fptr = fopen(metapathname,"r");
  setbuf(fptr,NULL);
  flock(fileno(fptr),LOCK_SH);

  if (search_subdir_only)
   {
    fseek(fptr,sizeof(struct stat), SEEK_SET);
    fread(&tempmeta,sizeof(DIR_META_TYPE),1,fptr);
    thisfile_pos = tempmeta.next_subdir_page;

    while(thisfile_pos != 0)
     {
      fseek(fptr, thisfile_pos, SEEK_SET);
      fread(&temp_page,sizeof(DIR_ENTRY_PAGE),1,fptr);
      for (count=0;count<temp_page.num_entries;count++)
       {
        if (strcmp(temp_page.dir_entries[count].d_name,target_entry_name)==0)
         {
          hit_inode = temp_page.dir_entries[count].d_ino;
          if ((prepath_length+strlen(target_entry_name))<256)
           {
            new_prepath_length = prepath_length+1+strlen(target_entry_name);
            strncpy(tempname,fullpath,new_prepath_length);
            tempname[new_prepath_length]=0;
            replace_pathname_cache(compute_hash(tempname),tempname,hit_inode);
           }
          flock(fileno(fptr),LOCK_UN);
          fclose(fptr);
          return lookup_pathname_recursive(hit_inode, new_prepath_length,&(fullpath[new_prepath_length]),fullpath, errcode);
         }
       }
      thisfile_pos = temp_page.next_page;
     }
    flock(fileno(fptr),LOCK_UN);
    fclose(fptr);
    *errcode = -ENOENT;
    return 0;   /*Cannot find this entry*/
   }

  strcpy(target_entry_name,&(partialpath[1]));

  fseek(fptr,sizeof(struct stat), SEEK_SET);
  ret_val=fread(&tempmeta,sizeof(DIR_META_TYPE),1,fptr);
  if (ret_val < 1)
   {
    *errcode = -EACCES;
    flock(fileno(fptr),LOCK_UN);
    fclose(fptr);
    return 0;
   }
  thisfile_pos = tempmeta.next_subdir_page;

  while(thisfile_pos != 0)
   {
    fseek(fptr, thisfile_pos, SEEK_SET);
    ret_val = fread(&temp_page,sizeof(DIR_ENTRY_PAGE),1,fptr);
    if (ret_val < 1)
     {
      *errcode = -EACCES;
      flock(fileno(fptr),LOCK_UN);
      fclose(fptr);
      return 0;
     }

    for (count=0;count<temp_page.num_entries;count++)
     {
      if (strcmp(temp_page.dir_entries[count].d_name,target_entry_name)==0)
       {
        hit_inode = temp_page.dir_entries[count].d_ino;
        if ((prepath_length+strlen(target_entry_name))<256)
         {
          new_prepath_length = prepath_length+1+strlen(target_entry_name);
          strncpy(tempname,fullpath,new_prepath_length);
          tempname[new_prepath_length]=0;
          replace_pathname_cache(compute_hash(tempname),tempname,hit_inode);
         }
        flock(fileno(fptr),LOCK_UN);
        fclose(fptr);
        return hit_inode;
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
      if (strcmp(temp_page.dir_entries[count].d_name,target_entry_name)==0)
       {
        hit_inode = temp_page.dir_entries[count].d_ino;
        if ((prepath_length+strlen(target_entry_name))<256)
         {
          new_prepath_length = prepath_length+1+strlen(target_entry_name);
          strncpy(tempname,fullpath,new_prepath_length);
          tempname[new_prepath_length]=0;
          replace_pathname_cache(compute_hash(tempname),tempname,hit_inode);
         }
        flock(fileno(fptr),LOCK_UN);
        fclose(fptr);
        return hit_inode;
       }
     }
    thisfile_pos = temp_page.next_page;
   }

  *errcode = -ENOENT;

  flock(fileno(fptr),LOCK_UN);
  fclose(fptr);
  return 0;
 }