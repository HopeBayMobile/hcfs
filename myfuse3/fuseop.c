/* Code under development by Jiahong Wu*/

#include "myfuse.h"
#include <math.h>

int mygetattr(const char *path, struct stat *nodestat)
 {

  int retcode=0;
  struct stat inputstat;
  ino_t this_inode;
  FILE *fptr;
  char metapath[1024];

  show_current_time();


  memset(nodestat,0,sizeof(struct stat));

  this_inode = find_inode(path);

  printf("Inode is %ld\n",this_inode);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
/*
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    fptr=fopen(metapath,"r");
    if (fptr==NULL)
     retcode = -ENOENT;
    else
     {
      fread((void*)&inputstat,sizeof(struct stat),1,fptr);
      fclose(fptr);
*/
     {
      retcode = super_inode_read(&inputstat,this_inode);
      if (retcode < 0)
       return retcode;
      nodestat->st_nlink=inputstat.st_nlink;
      nodestat->st_uid=inputstat.st_uid;
      nodestat->st_gid=inputstat.st_gid;
      nodestat->st_dev=inputstat.st_dev;
      nodestat->st_ino=inputstat.st_ino;
      nodestat->st_size=inputstat.st_size;
      nodestat->st_blksize=MAX_BLOCK_SIZE;
      nodestat->st_blocks=inputstat.st_blocks;
      nodestat->st_atime=inputstat.st_atime;
      nodestat->st_mtime=inputstat.st_mtime;
      nodestat->st_ctime=inputstat.st_ctime;
      nodestat->st_mode=inputstat.st_mode;
     }

   }

  return retcode;
 }   

int myreaddir(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi)
 {
  struct stat inputstat;
  int retcode=0;
  ino_t this_inode;
  FILE *fptr;
  char metapath[1024];
  long num_subdir,num_reg,count;
  simple_dirent tempent;
  char temppath[MAX_ICACHE_PATHLEN+10];
  unsigned int inodehash;

  show_current_time();


  this_inode = find_inode(path);

  if (this_inode <=0)
   {
    retcode = -ENOENT;
   }
  else
   {
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    fptr=fopen(metapath,"r");
    if (fptr==NULL)
     retcode = -ENOENT;
    else
     {
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fread(&num_subdir, sizeof(long),1,fptr);
      fread(&num_reg, sizeof(long),1,fptr);
      for(count=0;count<num_subdir+num_reg;count++)
       {
        fread(&tempent,sizeof(simple_dirent),1,fptr);

        if (tempent.st_ino>0)
         {
          memset(&inputstat,0,sizeof(struct stat));
          inputstat.st_ino=tempent.st_ino;
          inputstat.st_mode=tempent.st_mode;
          filler(buf, tempent.name, &inputstat, 0);
          if ((strlen(path)+strlen(tempent.name)+1)<MAX_ICACHE_PATHLEN)
           {
            sprintf(temppath,"%s/%s",path,tempent.name);
            inodehash = compute_inode_hash(temppath);
            replace_inode_cache(inodehash,temppath,tempent.st_ino);
           }

         }
        else
         filler(buf, tempent.name, NULL, 0);
       }

      fclose(fptr);
     }
   }
  return 0;
 }

int myopen(const char *path, struct fuse_file_info *fi)
 {
  int count,count2;
  uint64_t empty_index;
  uint64_t temp_mask;
  char metapath[1024];

  printf("Debug open path %s\n",path);
  show_current_time();


  if (num_opened_files > MAX_FILE_TABLE_SIZE)
   {
    fi -> fh = 0;
    return 0;
   }
  
  /*now find the empty file table slot*/
  sem_wait(&file_table_sem);
  empty_index=0;
  for(count=0;count<(MAX_FILE_TABLE_SIZE/64);count++)
   {
    temp_mask = ~opened_files_masks[count];
    if (temp_mask > 0)  /*have empty slot*/
     {
      temp_mask = 1;
      for(count2=0;count2<64;count2++)
       {
        if ((opened_files_masks[count] & temp_mask) ==0)
         {
          empty_index = count*64+count2+1;
          opened_files_masks[count] = opened_files_masks[count] | temp_mask;
          num_opened_files++;
          break;
         }
        if (count2 == 63)
         break;
        temp_mask = temp_mask << 1;
       }
     }
    else
     continue;
    if (empty_index > 0)
     break;
   }

  fi -> fh = empty_index;
  file_handle_table[empty_index].st_ino = find_inode(path);
  if (file_handle_table[empty_index].st_ino==0)
   {
    /*No such inode?*/
    num_opened_files--;
    temp_mask = ~temp_mask;
    opened_files_masks[count] = opened_files_masks[count] & temp_mask;
    fi -> fh =0;
    sem_post(&file_table_sem);
    return -ENOENT;
   }
  show_current_time();

  file_handle_table[empty_index].opened_block=0;
  file_handle_table[empty_index].blockptr=NULL;

  sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,
                                file_handle_table[empty_index].st_ino % SYS_DIR_WIDTH,file_handle_table[empty_index].st_ino);

  file_handle_table[empty_index].metaptr=fopen(metapath,"r+");
  printf("debug open metapath is %s\n",metapath);
  if (file_handle_table[empty_index].metaptr ==NULL)
   {
    /*No such inode?*/
    num_opened_files--;
    temp_mask = ~temp_mask;
    opened_files_masks[count] = opened_files_masks[count] & temp_mask;
    fi -> fh =0;
    sem_post(&file_table_sem);
    return -ENOENT;
   }
  
  sem_post(&file_table_sem);
  printf("Debug end of myopen\n");
  show_current_time();
  return 0;
 }
int myrelease(const char *path, struct fuse_file_info *fi)
 {
  uint64_t index;
  int count,count2;
  uint64_t temp_mask;

  if (fi->fh == 0)
   return 0;

  sem_wait(&file_table_sem);
  index = fi->fh;
  if (file_handle_table[index].metaptr!=NULL)
   fclose(file_handle_table[index].metaptr);
  if (file_handle_table[index].blockptr!=NULL)
   fclose(file_handle_table[index].blockptr);
  file_handle_table[index].opened_block = 0;
  file_handle_table[index].st_ino = 0;
  file_handle_table[index].metaptr=NULL;
  file_handle_table[index].blockptr=NULL;

  index=index-1;
  count = index / 64;
  count2 = index % 64;

  temp_mask = 1;
  temp_mask = temp_mask << count2;
  temp_mask = ~temp_mask;
  opened_files_masks[count] = opened_files_masks[count] & temp_mask;
  num_opened_files--;

  sem_post(&file_table_sem);
  return 0;
 } 

int myopendir(const char *path, struct fuse_file_info *fi)
 {
  show_current_time();

  return 0;
 }
int myread(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
 {
  int retsize=0;
  FILE *metaptr,*data_fptr;
  ino_t this_inode;
  char metapath[1024];
  char blockpath[1024];
  struct stat inputstat;
  long total_blocks,start_block,end_block,count;
  int total_read_bytes, max_to_read, have_error, actual_read_bytes;
  off_t current_offset,end_bytes;
  blockent tmp_block;

  show_current_time();

  printf("Debug myread path %s, size %ld, offset %ld\n",path,size,offset);

  if (size==0)
   return 0;

  if (fi->fh==0)
   {
    this_inode = find_inode(path);
    if (this_inode==0)
     return -ENOENT;

    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    metaptr=fopen(metapath,"r+");
    if (metaptr==NULL)
     return -ENOENT;
   }
  else
   {
    this_inode = file_handle_table[fi->fh].st_ino;
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    metaptr=fopen(metapath,"r+");
    if (metaptr==NULL)
     return -ENOENT;
   }

  fread(&inputstat,sizeof(struct stat),1,metaptr);
  fread(&total_blocks,sizeof(long),1,metaptr);

  if (offset >= inputstat.st_size)  /* If want to read outside file size */
   {
    fclose(metaptr);
    return 0;
   }
  total_read_bytes=0;

  current_offset=offset;
  end_bytes = (long)offset + (long)size -1;

  start_block = (offset / (long) MAX_BLOCK_SIZE) + 1;
  end_block = (end_bytes / (long) MAX_BLOCK_SIZE) + 1;  /*Assume that block_index starts from 1. */

  printf("%ld, %ld, %ld\n",start_block,end_block,end_bytes);
  printf("total block %ld\n",total_blocks);

  if (start_block > total_blocks)
   {
    fclose(metaptr);
    return 0;
   }

  fseek(metaptr,sizeof(struct stat)+sizeof(long)+(start_block-1)*sizeof(blockent),SEEK_SET); /*Seek to the first block to read on the meta*/

  printf("%ld, %ld\n",start_block,end_block);
  for(count=start_block;count<=end_block;count++)
   {
    if (count > total_blocks)
     {
      /*End of file encountered*/
      break;
     }
    else
     fread(&tmp_block,sizeof(blockent),1,metaptr);
    sprintf(blockpath,"%s/sub_%ld/data_%ld_%ld",BLOCKSTORE,(this_inode + count) % SYS_DIR_WIDTH,this_inode,count);

    if (tmp_block.stored_where==1)
     {
      data_fptr=fopen(blockpath,"r");
      if (data_fptr==NULL)
       {
        retsize = -1;
        break;
       }
     }
    else
     {
      /*Storage in cloud not implemeted now*/
      break;
     }
    printf("%s\n",blockpath);
    fseek(data_fptr,current_offset - (MAX_BLOCK_SIZE * (count-1)),SEEK_SET);
    if ((MAX_BLOCK_SIZE * count) < (offset+size))
     max_to_read = (MAX_BLOCK_SIZE * count) - current_offset;
    else
     max_to_read = (offset+size) - current_offset;
    actual_read_bytes = fread(&buf[current_offset-offset],sizeof(char),max_to_read,data_fptr);

    printf("Read debug max_to_read %d actual read %d\n",max_to_read, actual_read_bytes);

    total_read_bytes += actual_read_bytes;
    current_offset +=actual_read_bytes;
    if (actual_read_bytes < max_to_read)
     {
      have_error = ferror(data_fptr);
      fclose(data_fptr);
      if (have_error>0)
       {
        retsize = -have_error;
        break;
       }
      else
       break;
     }
    else
     {
      fclose(data_fptr);
     }
   }

  if (retsize>=0)
   retsize = total_read_bytes;
  printf("Debug myread end path %s, size %ld, offset %ld, total read %d\n",path,size,offset,retsize);
  fclose(metaptr);

  return retsize;
 }

int mywrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
 {
  int retsize=0;
  FILE *metaptr,*data_fptr;
  ino_t this_inode;
  char metapath[1024];
  char blockpath[1024];
  struct stat inputstat;
  long total_blocks,start_block,end_block,count;
  int total_write_bytes, max_to_write, have_error, actual_write_bytes;
  off_t current_offset,end_bytes;
  blockent tmp_block;

  show_current_time();

  printf("Debug mywrite path %s, size %ld, offset %ld\n",path,size,offset);

  if (size==0)
   return 0;

  if (fi->fh==0)
   {
    this_inode = find_inode(path);
    if (this_inode==0)
     return -ENOENT;

    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    metaptr=fopen(metapath,"r+");
    if (metaptr==NULL)
     return -ENOENT;
   }
  else
   {
    metaptr = file_handle_table[fi->fh].metaptr;
    this_inode = file_handle_table[fi->fh].st_ino;
    fseek(metaptr,0,SEEK_SET);
   }

  fread(&inputstat,sizeof(struct stat),1,metaptr);
  fread(&total_blocks,sizeof(long),1,metaptr);

  total_write_bytes=0;

  current_offset=offset;
  end_bytes = (long)offset + (long)size -1;

  start_block = (offset / (long) MAX_BLOCK_SIZE) + 1;
  end_block = (end_bytes / (long) MAX_BLOCK_SIZE) + 1;  /*Assume that block_index starts from 1. */

  printf("%ld, %ld, %ld\n",start_block,end_block,end_bytes);
  printf("total block %ld\n",total_blocks);

  if ((start_block -1) > total_blocks)  /*Padding the file meta*/
   {
    memset(&tmp_block,0,sizeof(blockent));
    fseek(metaptr,0,SEEK_END);
    for (count = 0;count<((start_block-1) - total_blocks); count++)
     {
      fwrite(&tmp_block,sizeof(blockent),1,metaptr);
     }
    total_blocks += ((start_block -1) - total_blocks);
   }
  fseek(metaptr,sizeof(struct stat)+sizeof(long)+(start_block-1)*sizeof(blockent),SEEK_SET); /*Seek to the first block to write on the meta*/
  printf("%ld, %ld\n",start_block,end_block);
  for(count=start_block;count<=end_block;count++)
   {
    if (count > total_blocks)
     {
      memset(&tmp_block,0,sizeof(blockent));
      total_blocks++;
     }
    else
     fread(&tmp_block,sizeof(blockent),1,metaptr);
    sprintf(blockpath,"%s/sub_%ld/data_%ld_%ld",BLOCKSTORE,(this_inode + count) % SYS_DIR_WIDTH,this_inode,count);

    if ((fi->fh>0) && (file_handle_table[fi->fh].opened_block == count))
     {
      data_fptr=file_handle_table[fi->fh].blockptr;
     }
    else
     {
      if ((fi->fh>0) && (file_handle_table[fi->fh].opened_block!=0))
       {
        fclose(file_handle_table[fi->fh].blockptr);
        file_handle_table[fi->fh].opened_block=0;
       }
      if (tmp_block.stored_where==1)
       {
        data_fptr=fopen(blockpath,"r+");
       }
      else
       {
        data_fptr=fopen(blockpath,"w+");
        tmp_block.stored_where=1;
       }
      if (fi->fh>0)
       {
        file_handle_table[fi->fh].opened_block=count;
        file_handle_table[fi->fh].blockptr=data_fptr;
       }
     }
    printf("%s\n",blockpath);
    fseek(data_fptr,current_offset - (MAX_BLOCK_SIZE * (count-1)),SEEK_SET);
    if ((MAX_BLOCK_SIZE * count) < (offset+size))
     max_to_write = (MAX_BLOCK_SIZE * count) - current_offset;
    else
     max_to_write = (offset+size) - current_offset;
    actual_write_bytes = fwrite(&buf[current_offset-offset],sizeof(char),max_to_write,data_fptr);
    total_write_bytes += actual_write_bytes;
    current_offset +=actual_write_bytes;
    fseek(metaptr,sizeof(struct stat)+sizeof(long)+(count-1)*sizeof(blockent),SEEK_SET);
    fwrite(&tmp_block,sizeof(blockent),1,metaptr);
    if (actual_write_bytes < max_to_write)
     {
      have_error = ferror(data_fptr);
      if (fi->fh>0)
       {
        file_handle_table[fi->fh].opened_block=0;
        file_handle_table[fi->fh].blockptr=NULL;
       }
      fclose(data_fptr);
      if (have_error>0)
       {
        retsize = -have_error;
        break;
       }
      else
       {
        printf("ERROR! Short written but have no ferror...\n");
        retsize = -1;
        break;
       }
     }
    else
     {
      if (fi->fh==0)
       fclose(data_fptr);
     }
   }
    
  if (inputstat.st_size < (offset+total_write_bytes))
   {
    mysystem_meta.system_size += (offset+total_write_bytes) - inputstat.st_size;
    inputstat.st_size = offset+total_write_bytes;
    inputstat.st_blocks = (inputstat.st_size+511)/512;
   }
  fseek(metaptr,0,SEEK_SET);
  fwrite(&inputstat,sizeof(struct stat),1,metaptr);
  fwrite(&total_blocks,sizeof(long),1,metaptr);

  super_inode_write(&inputstat,this_inode);


  if (retsize>=0)
   retsize = total_write_bytes;
  printf("Debug mywrite end path %s, size %ld, offset %ld, total write %d\n",path,size,offset,retsize); 
  if (fi->fh == 0)
   fclose(metaptr);
  return retsize;
 }

int mymknod(const char *path, mode_t filemode,dev_t thisdev)
 {
  struct stat inputstat;
  int retcode=0;
  ino_t this_inode,new_inode;
  FILE *fptr;
  char metapath[1024];
  long num_subdir,num_reg,count;
  simple_dirent tempent;
  char *tmpptr;
  long num_blocks =0;
  unsigned int inodehash;
  struct timeb currenttime;

  ftime(&currenttime);

  tmpptr = strrchr(path,'/');
  show_current_time();


  this_inode = find_parent_inode(path);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    fptr=fopen(metapath,"r+");
    if (fptr==NULL)
     retcode = -ENOENT;
    else
     {
      mysystem_meta.total_inodes +=1;
      mysystem_meta.max_inode+=1;
      new_inode = mysystem_meta.max_inode;

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
      strcpy(tempent.name,&path[tmpptr-path+1]);
      fwrite(&tempent,sizeof(simple_dirent),1,fptr);
      fclose(fptr);
      memset(&inputstat,0,sizeof(struct stat));
      inputstat.st_ino=new_inode;
      inputstat.st_mode = S_IFREG | 0755;
      inputstat.st_nlink = 1;
      inputstat.st_uid=getuid();
      inputstat.st_gid=getgid();
      inputstat.st_atime=currenttime.time;
      inputstat.st_mtime=currenttime.time;
      inputstat.st_ctime=currenttime.time;
      sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,new_inode % SYS_DIR_WIDTH,new_inode);

      fptr=fopen(metapath,"w");
      fwrite(&inputstat,sizeof(struct stat),1,fptr);
      num_blocks = 0;
      fwrite(&num_blocks,sizeof(long),1,fptr);
      fclose(fptr);
      super_inode_create(&inputstat,new_inode);

     }
   }

  if (strlen(path)<MAX_ICACHE_PATHLEN)
   {
    inodehash = compute_inode_hash(path);
    replace_inode_cache(inodehash,path,new_inode);
   }
  mysync_system_meta();
  show_current_time();

  return retcode;
 }

int mymkdir(const char *path,mode_t thismode)
 {
  struct stat inputstat;
  int retcode=0;
  ino_t this_inode,new_inode;
  FILE *fptr;
  char metapath[1024];
  long num_subdir,num_reg,count;
  simple_dirent tempent,ent1,ent2;
  char *tmpptr;
  struct timeb currenttime;

  ftime(&currenttime);

  show_current_time();

  tmpptr = strrchr(path,'/');

  this_inode = find_parent_inode(path);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    fptr=fopen(metapath,"r+");
    if (fptr==NULL)
     retcode = -ENOENT;
    else
     {
      mysystem_meta.total_inodes +=1;
      mysystem_meta.max_inode+=1;
      new_inode = mysystem_meta.max_inode;
      fread(&inputstat,sizeof(struct stat),1,fptr);
      inputstat.st_nlink++;
      inputstat.st_mtime=currenttime.time;
      fseek(fptr,0,SEEK_SET);
      fwrite(&inputstat,sizeof(struct stat),1,fptr);
      super_inode_write(&inputstat,this_inode);

      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fread(&num_subdir,sizeof(long),1,fptr);
      fread(&num_reg,sizeof(long),1,fptr);
      num_subdir++;
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fwrite(&num_subdir,sizeof(long),1,fptr);
      fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+((num_subdir-1)*sizeof(simple_dirent)),SEEK_SET);
      if (num_reg > 0)    /* We will need to swap the first regular file entry to the end of meta*/
       {
        fread(&tempent,sizeof(simple_dirent),1,fptr);
        fseek(fptr,0,SEEK_END);
        fwrite(&tempent,sizeof(simple_dirent),1,fptr);
        fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+((num_subdir-1)*sizeof(simple_dirent)),SEEK_SET);
       }

      memset(&tempent,0,sizeof(simple_dirent));
      tempent.st_ino = new_inode;
      tempent.st_mode = S_IFDIR | 0755;
      strcpy(tempent.name,&path[tmpptr-path+1]);
      fwrite(&tempent,sizeof(simple_dirent),1,fptr);
      fclose(fptr);
      /*Done with updating the parent inode meta*/


      memset(&inputstat,0,sizeof(struct stat));
      inputstat.st_ino=new_inode;
      inputstat.st_mode = S_IFDIR | 0755;
      inputstat.st_nlink = 2;
      inputstat.st_uid=getuid();
      inputstat.st_gid=getgid();
      inputstat.st_atime=currenttime.time;
      inputstat.st_mtime=currenttime.time;
      inputstat.st_ctime=currenttime.time;

      sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,new_inode % SYS_DIR_WIDTH,new_inode);

      fptr=fopen(metapath,"w");
      fwrite(&inputstat,sizeof(struct stat),1,fptr);
      num_subdir=2;
      num_reg=0;
      fwrite(&num_subdir,sizeof(long),1,fptr);
      fwrite(&num_reg,sizeof(long),1,fptr);
      memset(&ent1,0,sizeof(simple_dirent));
      memset(&ent2,0,sizeof(simple_dirent));
      ent1.st_ino=new_inode;
      ent1.st_mode=S_IFDIR | 0755;
      strcpy(ent1.name,".");
      ent2.st_ino=this_inode;
      ent2.st_mode=S_IFDIR | 0755;
      strcpy(ent2.name,"..");
      fwrite(&ent1,sizeof(simple_dirent),1,fptr);
      fwrite(&ent2,sizeof(simple_dirent),1,fptr);
      fclose(fptr);
      super_inode_create(&inputstat,new_inode);

     }
   }

  mysync_system_meta();
  return retcode;
 }


int myutime(const char *path, struct utimbuf *mymodtime)
 {

  struct stat inputstat;
  int retcode=0;
  ino_t this_inode;
  FILE *fptr;
  char metapath[1024];
  struct timeb currenttime;

  ftime(&currenttime);

  show_current_time();

  printf("Debug myutime\n");

  this_inode = find_inode(path);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    fptr = fopen(metapath,"r+");
    if (fptr==NULL)
     return -ENOENT;
    fread(&inputstat,sizeof(struct stat),1,fptr);
    if (mymodtime==NULL)
     {
      inputstat.st_atime=currenttime.time;
      inputstat.st_mtime=currenttime.time;
     }
    else
     {
      inputstat.st_atime=mymodtime->actime;
      inputstat.st_mtime=mymodtime->modtime;
     }
    fseek(fptr,0,SEEK_SET);
    fwrite(&inputstat,sizeof(struct stat),1,fptr);
    fclose(fptr);
    super_inode_write(&inputstat,this_inode);

   }

  return retcode;
 } 

int myrename(const char *oldname, const char *newname)
 {
  int retcode=0;
  show_current_time();

  return retcode;
 }

int myunlink(const char *path)
 {
/*Todo: need to revise this to take close after delete into account*/
/*TODO: will need to actually decrease n_link to inode, and to check if
  an opened file points to that inode. */
  struct stat inputstat;
  int retcode=0;
  ino_t parent_inode,this_inode;
  FILE *fptr;
  char metapath[1024];
  char blockpath[1024];
  long num_subdir,num_reg,total_blocks;
  simple_dirent tempent;
  char *tmpptr;
  int tmpstatus;
  int tmp_index,count;
  long block_count;

  show_current_time();

  printf("Debug myunlink: deleting %s\n",path);
  tmpptr = strrchr(path,'/');

  parent_inode = find_parent_inode(path);
  this_inode = find_inode(path);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    fptr = fopen(metapath,"r");
    if (fptr==NULL)
     return -ENOENT;
    fread(&inputstat,sizeof(struct stat),1,fptr);
    mysystem_meta.system_size -= inputstat.st_size;

    fread(&total_blocks,sizeof(long),1,fptr);
    fclose(fptr);
    invalidate_inode_cache(path);
    tmpstatus=unlink(metapath);
    retcode = super_inode_delete(this_inode);
    if (tmpstatus!=0)
     return -1;
    mysystem_meta.total_inodes -=1;

    /* Removing all blocks for this inode */
    for(block_count=1;block_count<=total_blocks;block_count++)
     {
      sprintf(blockpath,"%s/sub_%ld/data_%ld_%ld",BLOCKSTORE,
                                           (this_inode + block_count) % SYS_DIR_WIDTH,this_inode,block_count);

      unlink(blockpath);
     }

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
        printf("To this file %s, check %s\n",tempent.name,&path[(tmpptr-path)+1]);
        if (strcmp(tempent.name,&path[(tmpptr-path)+1])==0)
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
      printf("finished unlink\n");
     }
   }

   mysync_system_meta();


  return retcode;
 }
int myrmdir(const char *path)
 {
  struct stat inputstat;
  int retcode=0;
  ino_t parent_inode,this_inode;
  FILE *fptr;
  char metapath[1024];
  long num_subdir,num_reg;
  simple_dirent tempent;
  char *tmpptr;
  int tmpstatus;
  int tmp_index,count;

/* TODO: Need to check for if directory empty before proceeding*/

  show_current_time();

  printf("Debug myrmdir: deleting %s\n",path);

  tmpptr = strrchr(path,'/');

  parent_inode = find_parent_inode(path);
  this_inode = find_inode(path);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);


    /*First, check if the subdirectory is empty or not*/

    fptr=fopen(metapath,"r");
    if (fptr==NULL)
     retcode = -ENOENT;
    else
     {
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fread(&num_subdir,sizeof(long),1,fptr);
      fread(&num_reg,sizeof(long),1,fptr);
      fclose(fptr);
      if ((num_subdir+num_reg)>2)
       return -ENOTEMPTY;
     }



    /*Invalidate the inode cache*/
    invalidate_inode_cache(path);

    tmpstatus=unlink(metapath);
    retcode = super_inode_delete(this_inode);
    if (tmpstatus!=0)
     return -1;
    mysystem_meta.total_inodes -=1;

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
        printf("To this directory %s, check %s\n",tempent.name,&path[(tmpptr-path)+1]);
        if (strcmp(tempent.name,&path[(tmpptr-path)+1])==0)
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
   }

  mysync_system_meta();

  return retcode;
 }


int myfsync(const char *path, int datasync, struct fuse_file_info *fi)
 {
  show_current_time();

  return 0;
 }
int mytruncate(const char *path, off_t length)
 {
/* TODO: will need to handle truncate to larger file sizes (need to pad zeros?) */
  struct stat inputstat;
  int retcode=0;
  ino_t parent_inode,this_inode;
  FILE *fptr;
  char metapath[1024];
  char blockpath[1024];
  long num_subdir,num_reg,total_blocks,last_block;
  simple_dirent tempent;
  char *tmpptr;
  int tmpstatus;
  int tmp_index,count;
  long block_count;

  show_current_time();

  printf("Debug truncate path %s length %ld\n",path,length);

  this_inode = find_inode(path);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    fptr = fopen(metapath,"r+");
    if (fptr==NULL)
     return -ENOENT;
    fread(&inputstat,sizeof(struct stat),1,fptr);
    fread(&total_blocks,sizeof(long),1,fptr);
    if (length == 0)
     last_block = 0;
    else
     last_block = ((length-1) / (long) MAX_BLOCK_SIZE) + 1;

    printf("Debug truncate: last block is %ld\n",last_block);

    /*First delete blocks that need to be thrown away*/
    for(block_count=last_block+1;block_count<=total_blocks;block_count++)
     {
      printf("Debug truncate: killing block %ld",block_count);
      sprintf(blockpath,"%s/sub_%ld/data_%ld_%ld",BLOCKSTORE,
                                          (this_inode + block_count) % SYS_DIR_WIDTH,this_inode,block_count);

      unlink(blockpath);
     }
    if (length < (last_block * MAX_BLOCK_SIZE))
     {
      sprintf(blockpath,"%s/sub_%ld/data_%ld_%ld",BLOCKSTORE,
                       (this_inode + last_block) % SYS_DIR_WIDTH,this_inode,last_block);

      truncate(blockpath, length - ((last_block -1) * MAX_BLOCK_SIZE));
     }
    total_blocks = last_block;
    mysystem_meta.system_size += (length - inputstat.st_size);
    inputstat.st_size=length;
    inputstat.st_blocks = (inputstat.st_size+511)/512;
    fseek(fptr,0,SEEK_SET);
    fwrite(&inputstat,sizeof(struct stat),1,fptr);
    fwrite(&total_blocks,sizeof(long),1,fptr);
    fclose(fptr);
    super_inode_write(&inputstat,this_inode);

    truncate(metapath,sizeof(struct stat)+sizeof(long)+sizeof(blockent)*last_block);
   }
  return 0;
 }
int mystatfs(const char *path, struct statvfs *buf)
 {
  buf->f_bsize=4096;
  buf->f_namemax=256;
  if (mysystem_meta.total_inodes>1000000)
   buf->f_files=mysystem_meta.total_inodes*2;
  else
   buf->f_files = 2000000;
  buf->f_ffree=buf->f_files - mysystem_meta.total_inodes;
  buf->f_favail=buf->f_ffree;
  if (mysystem_meta.system_size > (50*powl(1024,3)))
   buf->f_blocks=(2*mysystem_meta.system_size)/4096;
  else
   buf->f_blocks=(100*powl(1024,3))/4096;
  buf->f_bfree=buf->f_blocks - ((mysystem_meta.system_size+4095)/4096);
  buf->f_bavail=buf->f_bfree;

  return 0;
 }
