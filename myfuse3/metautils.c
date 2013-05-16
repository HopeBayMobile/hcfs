/* Code under development by Jiahong Wu*/

#include "myfuse.h"
#include <math.h>



int decrease_nlink_ref(struct stat *inputstat)
 {
  FILE *fptr;
  int retcode,newretcode;
  char metapath[1024];
  char blockpath[1024];
  int tmpstatus;
  long block_count, total_blocks;


  if (inputstat->st_nlink==1)
   {
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,inputstat->st_ino % SYS_DIR_WIDTH,inputstat->st_ino);
    fptr = fopen(metapath,"r");
    fseek(fptr,sizeof(struct stat),SEEK_SET);
    fread(&total_blocks,sizeof(long),1,fptr);
    fclose(fptr);

    tmpstatus=unlink(metapath);
    retcode = super_inode_delete(inputstat->st_ino);
    if (tmpstatus!=0)
     return -1;
    sem_wait(&mysystem_meta_sem);
    mysystem_meta.system_size -= inputstat->st_size;
    mysystem_meta.total_inodes -=1;
    sem_post(&mysystem_meta_sem);

    for(block_count=1;block_count<=total_blocks;block_count++)
     {
      sprintf(blockpath,"%s/sub_%ld/data_%ld_%ld",BLOCKSTORE,
                                           (inputstat->st_ino + block_count) % SYS_DIR_WIDTH,inputstat->st_ino,block_count);

      unlink(blockpath);
     }

   }
  else
   {
    inputstat->st_nlink-=1;
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,inputstat->st_ino % SYS_DIR_WIDTH,inputstat->st_ino);
    fptr=fopen(metapath,"a+");
    fseek(fptr,0,SEEK_SET);
    fwrite(inputstat,sizeof(struct stat),1,fptr);
    fclose(fptr);
    super_inode_write(inputstat,inputstat->st_ino);
   }
  return 0;
 }

int dir_remove_filename(ino_t parent_inode, char* filename)
 {
  char metapath[1024];
  FILE *fptr;
  int retcode=0;
  long num_subdir,num_reg, count, tmp_index;
  simple_dirent tempent;

  sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,parent_inode % SYS_DIR_WIDTH,parent_inode);

  fptr=fopen(metapath,"r+");
  if (fptr==NULL)
   retcode = -ENOENT;
  else
   {
    fseek(fptr,sizeof(struct stat),SEEK_SET);
    fread(&num_subdir,sizeof(long),1,fptr);
    fread(&num_reg,sizeof(long),1,fptr);
    fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+(num_subdir*sizeof(simple_dirent)),SEEK_SET);

    for(count=0;count<num_reg;count++)
     {
      fread(&tempent,sizeof(simple_dirent),1,fptr);
      printf("To this file %s, check %s\n",tempent.name,filename);
      if (strcmp(tempent.name,filename)==0)
       {
        tmp_index = count;
        break;
       }
     }
    printf("Testing if have this file\n");
    if (count>=num_reg)
     return -ENOENT;
    num_reg--;
    fseek(fptr,sizeof(struct stat)+sizeof(long),SEEK_SET);
    fwrite(&num_reg,sizeof(long),1,fptr);
    if (tmp_index<num_reg)  /*If the entry to be deleted is not at the end of the meta*/
     {
      fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+((num_reg+num_subdir)*sizeof(simple_dirent)),SEEK_SET);
      fread(&tempent,sizeof(simple_dirent),1,fptr);
      fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+((tmp_index+num_subdir)*sizeof(simple_dirent)),SEEK_SET);
      fwrite(&tempent,sizeof(simple_dirent),1,fptr);
     }
    fclose(fptr);
    truncate(metapath,sizeof(struct stat)+(2*sizeof(long))+((num_reg+num_subdir)*sizeof(simple_dirent)));
   }
  return retcode;
 }

int dir_remove_dirname(ino_t parent_inode, char* dirname)
 {
  struct stat inputstat;
  int retcode=0;
  FILE *fptr;
  char metapath[1024];
  long num_subdir,num_reg;
  simple_dirent tempent;
  char *tmpptr;
  int tmpstatus;
  int tmp_index,count;

  sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,parent_inode % SYS_DIR_WIDTH,parent_inode);


  fptr=fopen(metapath,"r+");
  if (fptr==NULL)
   retcode = -ENOENT;
  else
   {
    fread(&inputstat,sizeof(struct stat),1,fptr);
    inputstat.st_nlink--;
    fseek(fptr,0,SEEK_SET);
    fwrite(&inputstat,sizeof(struct stat),1,fptr);
    super_inode_write(&inputstat,parent_inode);


    fseek(fptr,sizeof(struct stat),SEEK_SET);
    fread(&num_subdir,sizeof(long),1,fptr);
    fread(&num_reg,sizeof(long),1,fptr);
    fseek(fptr,sizeof(struct stat)+(2*sizeof(long)),SEEK_SET);
    for(count=0;count<num_subdir;count++)
     {
      fread(&tempent,sizeof(simple_dirent),1,fptr);
      printf("To this directory %s, check %s\n",tempent.name,dirname);
      if (strcmp(tempent.name,dirname)==0)
       {
        tmp_index = count;
        break;
       }
     }
    printf("Testing if have this directory\n");
    if (count>=num_subdir)
     return -ENOENT;
    num_subdir--;
    fseek(fptr,sizeof(struct stat),SEEK_SET);
    fwrite(&num_subdir,sizeof(long),1,fptr);
    if (tmp_index<num_subdir)  /*If the entry to be deleted is not at the end of the subdir meta*/
     {
      fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+(num_subdir*sizeof(simple_dirent)),SEEK_SET);  /*Last subdir entry*/
      fread(&tempent,sizeof(simple_dirent),1,fptr);
      fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+(tmp_index*sizeof(simple_dirent)),SEEK_SET);
      fwrite(&tempent,sizeof(simple_dirent),1,fptr);
     }
    if (num_reg > 0) /*If have reg file, also need to swap*/
    fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+((num_subdir+num_reg)*sizeof(simple_dirent)),SEEK_SET);  /*Last file entry*/
    fread(&tempent,sizeof(simple_dirent),1,fptr);
    fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+(num_subdir*sizeof(simple_dirent)),SEEK_SET); /*Overwrite the place occupied by the original last subdir entry*/
    fwrite(&tempent,sizeof(simple_dirent),1,fptr);

    fclose(fptr);
    truncate(metapath,sizeof(struct stat)+(2*sizeof(long))+((num_reg+num_subdir)*sizeof(simple_dirent)));
    printf("finished rmdir\n");
   }
  return retcode;
 }

int dir_add_filename(ino_t this_inode, ino_t new_inode, char *filename)
 {
  char metapath[1024];
  FILE *fptr;
  struct stat inputstat;
  long num_reg;
  simple_dirent tempent;
  struct timeb currenttime;

  ftime(&currenttime);

  sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

  fptr=fopen(metapath,"r+");
  if (fptr==NULL)
   return -ENOENT;

  fread(&inputstat,sizeof(struct stat),1,fptr);
  inputstat.st_mtime=currenttime.time;

  fseek(fptr,sizeof(struct stat)+sizeof(long),SEEK_SET);
  fread(&num_reg,sizeof(long),1,fptr);
  num_reg++;
  fseek(fptr,0,SEEK_SET);
  fwrite(&inputstat,sizeof(struct stat),1,fptr);
  super_inode_write(&inputstat,this_inode);


  fseek(fptr,sizeof(struct stat)+sizeof(long),SEEK_SET);
  fwrite(&num_reg,sizeof(long),1,fptr);
  fseek(fptr,0,SEEK_END);
  memset(&tempent,0,sizeof(simple_dirent));
  tempent.st_ino = new_inode;
  tempent.st_mode = S_IFREG | 0755;
  strcpy(tempent.name,filename);
  fwrite(&tempent,sizeof(simple_dirent),1,fptr);
  fclose(fptr);
  return 0;
 }

