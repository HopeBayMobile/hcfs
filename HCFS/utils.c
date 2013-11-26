#include "fuseop.h"
#include "params.h"
#include <unistd.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>

void fetch_meta_path(char *pathname, ino_t this_inode)   /*Will copy the filename of the meta file to pathname*/
 {
  char tempname[400];
  int sub_dir;

  sub_dir = this_inode % NUMSUBDIR;
  sprintf(tempname,"%s/sub_%d",METAPATH,sub_dir);
  if (access(tempname,F_OK)==-1)
   mkdir(tempname,0700);
  sprintf(tempname,"%s/sub_%d/meta%ld",METAPATH,sub_dir,this_inode);
  strcpy(pathname,tempname);
  return;
 }
void fetch_block_path(char *pathname, ino_t this_inode, long block_num)   /*Will copy the filename of the block file to pathname*/
 {
  char tempname[400];
  int sub_dir;

  sub_dir = (this_inode + block_num) % NUMSUBDIR;
  sprintf(tempname,"%s/sub_%d",BLOCKPATH,sub_dir);
  if (access(tempname,F_OK)==-1)
   mkdir(tempname,0700);
  sprintf(tempname,"%s/sub_%d/block%ld_%ld",BLOCKPATH,sub_dir,this_inode,block_num);
  strcpy(pathname,tempname);
  return;
 }

void parse_parent_self(const char *pathname, char *parentname, char *selfname)
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
  return;
 }

