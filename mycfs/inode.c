/* Code under development by Jiahong Wu*/

#include "myfuse.h"

unsigned int compute_inode_hash(const char *path)
 {
  int count;
  unsigned int hash;

  show_current_time();
  hash = 0;

  for(count=0;count<strlen(path);count++)
   {
    hash += ((unsigned int)path[count]) * rand_r(&hash);
    if (hash > MAX_ICACHE_ENTRY)
     hash = hash % MAX_ICACHE_ENTRY;
   }

  if (hash >= MAX_ICACHE_ENTRY)
   hash = 0;

  printf("Debug path %s, hash %d\n",path,hash);
  return hash;
 }

void replace_inode_cache(unsigned int inodehash,const char *fullpath, ino_t st_ino)
 {
  printf("debug replacing icache %d with %s, inode %ld\n",inodehash, fullpath, st_ino);
  sem_wait(&(path_cache[inodehash].cache_sem));
  strcpy(path_cache[inodehash].pathname,fullpath);
  path_cache[inodehash].st_ino = st_ino;
  sem_post(&(path_cache[inodehash].cache_sem));
 }

ino_t find_inode_fullpath(const char *path, ino_t this_inode, const char *fullpath, unsigned int inodehash)
 {
  char *ptr;
  char subname[400];
  FILE *fptr;
  char metapath[1024];
  long num_subdir;
  long num_reg;
  long count;
  simple_dirent tempent[500];
  simple_dirent *tempent_ptr;
  int num_tempent;
  int next_tempent;
  long tempent_index;
  int tempent_toread;

  printf("find inode %s full %s\n",path,fullpath);
  ptr=strchr(path,'/');

  if (ptr==NULL)
   {
    strcpy(subname,path);

    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    printf("debug find inode %s\n",metapath);

    fptr=fopen(metapath,"r");
    if (fptr==NULL)
     return 0;
    fseek(fptr,sizeof(struct stat),SEEK_SET);
    fread(&num_subdir, sizeof(long),1,fptr);
    fread(&num_reg, sizeof(long),1,fptr);
//    printf("Parent inode %ld, num sub %ld, num reg %ld\n",this_inode,num_subdir,num_reg);
    num_tempent=0;
    next_tempent=0;
    tempent_index=0;
    for(count=0;count<(num_subdir+num_reg);count++)
     {
      if (next_tempent >= num_tempent)
       {
        next_tempent = 0;
        if (((num_subdir+num_reg)-tempent_index) < 500)
         tempent_toread=((num_subdir+num_reg)-tempent_index);
        else
         tempent_toread = 500;
        tempent_index += tempent_toread;
        num_tempent = tempent_toread;
        fread(&tempent,sizeof(simple_dirent),tempent_toread,fptr);
       }
//      fread(&tempent,sizeof(simple_dirent),1,fptr);
//      printf("sub name %s\n",tempent.name);
      tempent_ptr=&(tempent[next_tempent]);
      next_tempent++;
//      printf("Index %d, num tempent %d, name %s\n",next_tempent-1,num_tempent,tempent_ptr->name);
//      show_current_time();
      if (strcmp(subname,tempent_ptr->name)==0)
       {
        fclose(fptr);
        if (strlen(fullpath)<MAX_ICACHE_PATHLEN)
         replace_inode_cache(inodehash,fullpath,tempent_ptr->st_ino);
        return tempent_ptr->st_ino;
       }
     }
    fclose(fptr);
    return 0;
   }
  else
   {
    strncpy(subname,path,(ptr-path));
    subname[(ptr-path)]=0;

    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);


    fptr=fopen(metapath,"r");
    if (fptr==NULL)
     return 0;
    fseek(fptr,sizeof(struct stat),SEEK_SET);
    fread(&num_subdir, sizeof(long),1,fptr);
    fread(&num_reg, sizeof(long),1,fptr);
    for(count=0;count<num_subdir;count++)
     {
      fread(&(tempent[0]),sizeof(simple_dirent),1,fptr);
      if (strcmp(subname,tempent[0].name)==0)
       {
        fclose(fptr);
        return find_inode_fullpath(ptr+1,tempent[0].st_ino, fullpath, inodehash);
       }
     }
    /* No need to check the reg files. This pathname is a directory.*/
    fclose(fptr);
    return 0;
   }
  return 0;
 }

void invalidate_inode_cache(const char *path)
 {
  unsigned int inodehash;

  if (strcmp(path,"/")==0)
   return;

  printf("debug start invalidate inode cache: path %s\n",path);

  inodehash=compute_inode_hash(path);

  sem_wait(&(path_cache[inodehash].cache_sem));
  if ((path_cache[inodehash].st_ino>0) && (strcmp(path,path_cache[inodehash].pathname)==0))
   {
    path_cache[inodehash].st_ino=0;
    path_cache[inodehash].pathname[0]=0;
    printf("Debug: found and invalidate inode hash entry for %s\n",path);
   }

  sem_post(&(path_cache[inodehash].cache_sem));
  return;
 }

ino_t find_inode(const char *path)
 {
  ino_t this_inode;
  int count;
  char temp_pathname[1024];
  ino_t temp_inode;
  unsigned int inodehash, inodehash_org;

  show_current_time();
  if (strcmp(path,"/")==0)
   return 1;

  printf("find inode start: path %s\n",path);

  inodehash_org = 0;
  if (strlen(path)<MAX_ICACHE_PATHLEN)
   {
    inodehash_org = compute_inode_hash(path);
    sem_wait(&(path_cache[inodehash_org].cache_sem));
    printf("debug hash comp %d: in cache %s, this path %s\n",inodehash,path_cache[inodehash_org].pathname,path);
    if (strcmp(path,path_cache[inodehash_org].pathname)==0)
     {
      printf("Hit, inode %ld\n",path_cache[inodehash_org].st_ino);
      this_inode=path_cache[inodehash_org].st_ino;
      sem_post(&(path_cache[inodehash_org].cache_sem));
      return this_inode;
     }
    sem_post(&(path_cache[inodehash_org].cache_sem));
   }

  for(count=(strlen(path)-1);count>0;count--)
   {
    if ((count >= MAX_ICACHE_PATHLEN) || (path[count]!='/'))
     continue;
    strncpy(temp_pathname,path,count);
    temp_pathname[count]=0;
    inodehash = compute_inode_hash(temp_pathname);

    sem_wait(&(path_cache[inodehash].cache_sem));
    if (path_cache[inodehash].st_ino ==0)
     {
      sem_post(&(path_cache[inodehash].cache_sem));
      continue;
     }

    if (strcmp(path_cache[inodehash].pathname,temp_pathname)==0)
     {
      temp_inode = path_cache[inodehash].st_ino;
      sem_post(&(path_cache[inodehash].cache_sem));
      this_inode=find_inode_fullpath(&path[count+1],temp_inode,path, inodehash_org);
      return this_inode;
     }
    sem_post(&(path_cache[inodehash].cache_sem));
   }
  this_inode=find_inode_fullpath(&path[1],1,path, inodehash_org);
  return this_inode;
 }

ino_t find_parent_inode(const char *path)
 {
  /* Find the inode of the parent of "path"*/
  ino_t this_inode;
  char *parentname;
  char *tmpptr;

  tmpptr = strrchr(path,'/');
  if (tmpptr==path)
   {
    this_inode=1;
   }
  else
   {
    parentname=malloc((tmpptr-path)+10);
    if (parentname == NULL)
     return 0;
    strncpy(parentname,path,(tmpptr-path));
    parentname[(tmpptr-path)] = 0;

    this_inode = find_inode(parentname);

    free(parentname);
   }

  return this_inode;
 }

