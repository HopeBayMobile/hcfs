#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <attr/xattr.h>

#include "utils.h"
#include "global.h"
#include "fuseop.h"
#include "params.h"

SYSTEM_CONF_STRUCT system_config;

int fetch_meta_path(char *pathname, ino_t this_inode)   /*Will copy the filename of the meta file to pathname*/
 {
  char tempname[METAPATHLEN];
  int sub_dir;
  int ret_code=0;

  if (METAPATH == NULL)
   return -1;

  if (access(METAPATH,F_OK)==-1)
   {
    ret_code = mkdir(METAPATH,0700);
    if (ret_code < 0)
     return ret_code;
   }

  sub_dir = this_inode % NUMSUBDIR;
  sprintf(tempname,"%s/sub_%d",METAPATH,sub_dir);
  if (access(tempname,F_OK)==-1)
   {
    ret_code = mkdir(tempname,0700);

    if (ret_code < 0)
     return ret_code;
   }

  sprintf(tempname,"%s/sub_%d/meta%lld",METAPATH,sub_dir,this_inode);
  strcpy(pathname,tempname);

  ret_code = 0;
  return ret_code;
 }
int fetch_todelete_path(char *pathname, ino_t this_inode)   /*Will copy the filename of the meta file in todelete folder to pathname*/
 {
  char tempname[400];
  int sub_dir;

  sub_dir = this_inode % NUMSUBDIR;
  sprintf(tempname,"%s/todelete",METAPATH);
  if (access(tempname,F_OK)==-1)
   mkdir(tempname,0700);
  sprintf(tempname,"%s/todelete/sub_%d",METAPATH,sub_dir);
  if (access(tempname,F_OK)==-1)
   mkdir(tempname,0700);
  sprintf(tempname,"%s/todelete/sub_%d/meta%lld",METAPATH,sub_dir,this_inode);
  strcpy(pathname,tempname);
  return 0;
 }
int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)   /*Will copy the filename of the block file to pathname*/
 {
  char tempname[400];
  int sub_dir;

  sub_dir = (this_inode + block_num) % NUMSUBDIR;
  sprintf(tempname,"%s/sub_%d",BLOCKPATH,sub_dir);
  if (access(tempname,F_OK)==-1)
   mkdir(tempname,0700);
  sprintf(tempname,"%s/sub_%d/block%lld_%lld",BLOCKPATH,sub_dir,this_inode,block_num);
  strcpy(pathname,tempname);
  return 0;
 }

int parse_parent_self(const char *pathname, char *parentname, char *selfname)
 {
  int count;

  for(count = strlen(pathname)-1;count>=0;count--)
   {
    if ((pathname[count]=='/') && (count < (strlen(pathname)-1)))
     break;
   }
  if (count ==0)
   {
    strcpy(parentname,"/");
    strcpy(selfname,&(pathname[1]));
   }
  else
   {
    strncpy(parentname,pathname,count);
    parentname[count]=0;
    strcpy(selfname,&(pathname[count+1]));
   }
  return 0;
 }

int read_system_config(char *config_path)
 {
  FILE *fptr;
  char tempbuf[200],*ret_ptr;
  char argname[200],argval[200],*tokptr1,*tokptr2, *toktmp, *strptr;

  fptr=fopen(config_path,"r");

  if (fptr==NULL)
   {
    printf("Cannot open config file (%s) for reading\n",config_path);
    exit(-1);
   }

  while(!feof(fptr))
   {
    ret_ptr = fgets(tempbuf,180,fptr);
    if (ret_ptr == NULL)
     break;
    
    if (strlen(tempbuf) > 170)
     {
      printf("Length of option value exceeds limit (170 chars). Exiting.\n");
      exit(-1);
     }
    if (tempbuf[strlen(tempbuf)-1]=='\n')
     tempbuf[strlen(tempbuf)-1]=0;

    /*Now decompose the option line into param name and value*/
    
    toktmp=strtok_r(tempbuf,"=",&tokptr1);

    if (toktmp==NULL)
     continue;

    /*Get rid of the leading and trailing space chars*/

    strptr=toktmp;
    while(*strptr==' ')
     strptr=strptr+sizeof(char);

    strcpy(argname,strptr);
    while(argname[strlen(argname)-1]==' ')
     argname[strlen(argname)-1]=0;

    /*Continue with the param value*/
    toktmp=strtok_r(NULL,"=",&tokptr1);
    if (toktmp==NULL)
     continue;

    strptr=toktmp;
    while(*strptr==' ')
     strptr=strptr+sizeof(char);
    strcpy(argval,strptr);
    while(argval[strlen(argval)-1]==' ')
     argval[strlen(argval)-1]=0;

    /*Match param name with required params*/
    if (strcasecmp(argname,"metapath")==0)
     {
      METAPATH= (char *) malloc(strlen(argval)+10);
      strcpy(METAPATH,argval);
      continue;
     }
    if (strcasecmp(argname,"blockpath")==0)
     {
      BLOCKPATH= (char *) malloc(strlen(argval)+10);
      strcpy(BLOCKPATH,argval);
      continue;
     }
    if (strcasecmp(argname,"superblock")==0)
     {
      SUPERBLOCK= (char *) malloc(strlen(argval)+10);
      strcpy(SUPERBLOCK,argval);
      continue;
     }
    if (strcasecmp(argname,"unclaimedfile")==0)
     {
      UNCLAIMEDFILE= (char *) malloc(strlen(argval)+10);
      strcpy(UNCLAIMEDFILE,argval);
      continue;
     }
    if (strcasecmp(argname,"hcfssystem")==0)
     {
      HCFSSYSTEM= (char *) malloc(strlen(argval)+10);
      strcpy(HCFSSYSTEM,argval);
      continue;
     }
    if (strcasecmp(argname,"cache_soft_limit")==0)
     {
      CACHE_SOFT_LIMIT=atoll(argval);
      continue;
     }
    if (strcasecmp(argname,"cache_hard_limit")==0)
     {
      CACHE_HARD_LIMIT=atoll(argval);
      continue;
     }
    if (strcasecmp(argname,"cache_delta")==0)
     {
      CACHE_DELTA=atoll(argval);
      continue;
     }
    if (strcasecmp(argname,"max_block_size")==0)
     {
      MAX_BLOCK_SIZE=atoll(argval);
      continue;
     }
    
   }
  fclose(fptr);
  
  return 0;
 }

int validate_system_config()
 {
  FILE *fptr;
  char pathname[400];
  char tempval[10];
  int ret_val;

  printf("%s 1\n%s 2\n%s 3\n%s 4\n%s 5\n", METAPATH,BLOCKPATH, SUPERBLOCK, UNCLAIMEDFILE, HCFSSYSTEM);
  printf("%lld %lld %lld %lld\n", CACHE_SOFT_LIMIT, CACHE_HARD_LIMIT, CACHE_DELTA, MAX_BLOCK_SIZE);

  sprintf(pathname,"%s/testfile",BLOCKPATH);

  fptr=fopen(pathname,"w");
  fprintf(fptr,"test\n");
  fclose(fptr);

  ret_val = setxattr(pathname,"user.dirty","T",1,0);
  if (ret_val < 0)
   {
    printf("Needs support for extended attributes, error no: %d\n",errno);
    exit(-1);
   }

  tempval[0]=0;
  getxattr(pathname,"user.dirty",(void *)tempval,1);
  printf("test value is: %s, %d\n",tempval,strncmp(tempval,"T",1));
  unlink(pathname);



  return 0;
 }



off_t check_file_size(const char *path)
 {
  struct stat block_stat;

  if (stat(path,&block_stat)==0)
   return block_stat.st_size;
  else
   return -1;
 }


