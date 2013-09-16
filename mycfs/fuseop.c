/* Code under development by Jiahong Wu*/

/*TODO: In operations, need to update block location flags */

#include "myfuse.h"
#include <math.h>

long check_file_size(const char *path)
 {
  struct stat block_stat;

  if (stat(path,&block_stat)==0)
   return block_stat.st_size;
  else
   return -1;
 }

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
    retcode = super_inode_read(&inputstat,this_inode);
    if (retcode < 0)
     return retcode;
    memcpy(nodestat,&inputstat,sizeof(struct stat));
    nodestat->st_blksize=MAX_BLOCK_SIZE;
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
         {
          if (strlen(tempent.name)>0)
           filler(buf, tempent.name, NULL, 0);
         }
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
  setbuf(file_handle_table[empty_index].metaptr,NULL);
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

  sem_init(&(file_handle_table[empty_index].meta_sem),0,1);
  sem_init(&(file_handle_table[empty_index].block_sem),0,1);

  fread(&(file_handle_table[empty_index].inputstat),sizeof(struct stat),1,file_handle_table[empty_index].metaptr);
  fread(&(file_handle_table[empty_index].total_blocks),sizeof(long),1,file_handle_table[empty_index].metaptr);

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
  sem_wait(&(file_handle_table[index].meta_sem));
  sem_wait(&(file_handle_table[index].block_sem));

  if (file_handle_table[index].metaptr!=NULL)
   fclose(file_handle_table[index].metaptr);
  if (file_handle_table[index].blockptr!=NULL)
   fclose(file_handle_table[index].blockptr);

  sem_post(&(file_handle_table[index].meta_sem));
  sem_post(&(file_handle_table[index].block_sem));

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

//  printf("Debug myread path %s, size %ld, offset %ld\n",path,size,offset);

  if (size==0)
   return 0;

  if (fi->fh==0)
   {
    this_inode = find_inode(path);
    if (this_inode==0)
     return -ENOENT;

    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    metaptr=fopen(metapath,"r+");
    setbuf(metaptr,NULL);

    if (metaptr==NULL)
     return -ENOENT;
   }
  else
   {
    metaptr = file_handle_table[fi->fh].metaptr;
    this_inode = file_handle_table[fi->fh].st_ino;
   }

  if (fi->fh==0)
   {
    fread(&inputstat,sizeof(struct stat),1,metaptr);
    fread(&total_blocks,sizeof(long),1,metaptr);
   }
  else
   {
    memcpy(&inputstat, &(file_handle_table[fi->fh].inputstat),sizeof(struct stat));
    total_blocks = file_handle_table[fi->fh].total_blocks;
   }

  if (offset >= inputstat.st_size)  /* If want to read outside file size */
   {
    if (fi->fh==0)
     fclose(metaptr);

    return 0;
   }
  total_read_bytes=0;

  current_offset=offset;
  end_bytes = (long)offset + (long)size -1;

  start_block = (offset / (long) MAX_BLOCK_SIZE) + 1;
  end_block = (end_bytes / (long) MAX_BLOCK_SIZE) + 1;  /*Assume that block_index starts from 1. */

  if (start_block > total_blocks)
   {
    if (fi->fh==0)
     fclose(metaptr);
    return 0;
   }

  for(count=start_block;count<=end_block;count++)
   {
    if (count > total_blocks)
     {
      /*End of file encountered*/
      break;
     }
    if (fi->fh > 0)
     sem_wait(&(file_handle_table[fi->fh].meta_sem));
    fseek(metaptr,sizeof(struct stat)+sizeof(long)+(count-1)*sizeof(blockent),SEEK_SET); /* Seek to the current block on the meta */
    fread(&tmp_block,sizeof(blockent),1,metaptr);
    if (fi->fh > 0)
     sem_post(&(file_handle_table[fi->fh].meta_sem));

    sprintf(blockpath,"%s/sub_%ld/data_%ld_%ld",BLOCKSTORE,(this_inode + count) % SYS_DIR_WIDTH,this_inode,count);


    if (fi->fh>0)
     sem_wait(&(file_handle_table[fi->fh].block_sem));

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
      if (tmp_block.stored_where % 2 ==1)
       {
        data_fptr=fopen(blockpath,"r+");
        setbuf(data_fptr,NULL);
        if (data_fptr==NULL)
         {
          retsize = -1;
          if (fi->fh>0)
           {
            sem_post(&(file_handle_table[fi->fh].block_sem));
           }
          break;
         }
       }
      else
       {
        /*Storage in cloud not implemeted now*/

        if (fi->fh>0)
         {
          sem_post(&(file_handle_table[fi->fh].block_sem));
         }
        break;
       }
      if (fi->fh>0)
       {
        file_handle_table[fi->fh].opened_block=count;
        file_handle_table[fi->fh].blockptr=data_fptr;
       }
     }

//    printf("%s\n",blockpath);
    fseek(data_fptr,current_offset - (MAX_BLOCK_SIZE * (count-1)),SEEK_SET);
    if ((MAX_BLOCK_SIZE * count) < (offset+size))
     max_to_read = (MAX_BLOCK_SIZE * count) - current_offset;
    else
     max_to_read = (offset+size) - current_offset;
    actual_read_bytes = fread(&buf[current_offset-offset],sizeof(char),max_to_read,data_fptr);

//    printf("Read debug max_to_read %d actual read %d\n",max_to_read, actual_read_bytes);

    total_read_bytes += actual_read_bytes;
    current_offset +=actual_read_bytes;
    if (actual_read_bytes < max_to_read)
     {
      have_error = ferror(data_fptr);
      if (fi->fh==0)
       fclose(data_fptr);
      else
       {
        sem_post(&(file_handle_table[fi->fh].block_sem));
       }
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
      if (fi->fh==0)
       fclose(data_fptr);
      else
       {
        sem_post(&(file_handle_table[fi->fh].block_sem));
       }
     }
   }

  if (retsize>=0)
   retsize = total_read_bytes;
//  printf("Debug myread end path %s, size %ld, offset %ld, total read %d\n",path,size,offset,retsize);
  if (fi->fh==0)
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
  long old_cache_size, new_cache_size;

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
    setbuf(metaptr,NULL);
    if (metaptr==NULL)
     return -ENOENT;
   }
  else
   {
    metaptr = file_handle_table[fi->fh].metaptr;
    this_inode = file_handle_table[fi->fh].st_ino;
   }

  printf("Debug write: writing to inode %ld\n", this_inode);

  flock(fileno(metaptr),LOCK_EX);

  if (fi->fh==0)
   {
    fread(&inputstat,sizeof(struct stat),1,metaptr);
    fread(&total_blocks,sizeof(long),1,metaptr);
   }
  else
   {
    memcpy(&inputstat, &(file_handle_table[fi->fh].inputstat),sizeof(struct stat));
    total_blocks = file_handle_table[fi->fh].total_blocks;
   }

  total_write_bytes=0;

  current_offset=offset;
  end_bytes = (long)offset + (long)size -1;

  start_block = (offset / (long) MAX_BLOCK_SIZE) + 1;
  end_block = (end_bytes / (long) MAX_BLOCK_SIZE) + 1;  /*Assume that block_index starts from 1. */

  printf("%ld, %ld, %ld\n",start_block,end_block,end_bytes);
  printf("total block %ld\n",total_blocks);

  if (fi->fh > 0)
   sem_wait(&(file_handle_table[fi->fh].meta_sem));

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

    if (fi->fh>0)
     sem_wait(&(file_handle_table[fi->fh].block_sem));

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
      if (tmp_block.stored_where % 2==1)
       {
        data_fptr=fopen(blockpath,"r+");
        setbuf(data_fptr,NULL);
        tmp_block.stored_where=1;
       }
      else
       {
        data_fptr=fopen(blockpath,"w+");
        setbuf(data_fptr,NULL);
        tmp_block.stored_where=1;
       }
      if (fi->fh>0)
       {
        file_handle_table[fi->fh].opened_block=count;
        file_handle_table[fi->fh].blockptr=data_fptr;
       }
     }
    printf("%s\n",blockpath);
    old_cache_size = check_file_size(blockpath);
    fseek(data_fptr,current_offset - (MAX_BLOCK_SIZE * (count-1)),SEEK_SET);
    if ((MAX_BLOCK_SIZE * count) < (offset+size))
     max_to_write = (MAX_BLOCK_SIZE * count) - current_offset;
    else
     max_to_write = (offset+size) - current_offset;
    actual_write_bytes = fwrite(&buf[current_offset-offset],sizeof(char),max_to_write,data_fptr);
    total_write_bytes += actual_write_bytes;
    current_offset +=actual_write_bytes;

    new_cache_size = check_file_size(blockpath);

    if (old_cache_size != new_cache_size)
     {
      sem_wait(&mysystem_meta_sem);
      mysystem_meta.cache_size += new_cache_size - old_cache_size;
      if (mysystem_meta.cache_size < 0)
       mysystem_meta.cache_size = 0;
      sem_post(&mysystem_meta_sem);
     }
    

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
    if (fi->fh>0)
     sem_post(&(file_handle_table[fi->fh].block_sem));
   }
    
  if (inputstat.st_size < (offset+total_write_bytes))
   {
    sem_wait(&mysystem_meta_sem);
    mysystem_meta.system_size += (offset+total_write_bytes) - inputstat.st_size;
    sem_post(&mysystem_meta_sem);
    inputstat.st_size = offset+total_write_bytes;
    inputstat.st_blocks = (inputstat.st_size+511)/512;
   }
  fseek(metaptr,0,SEEK_SET);
  fwrite(&inputstat,sizeof(struct stat),1,metaptr);
  fwrite(&total_blocks,sizeof(long),1,metaptr);

  if (fi->fh > 0)
   {
    memcpy(&(file_handle_table[fi->fh].inputstat),&inputstat,sizeof(struct stat));
    file_handle_table[fi->fh].total_blocks = total_blocks;
   }


  super_inode_write(&inputstat,this_inode);

  flock(fileno(metaptr),LOCK_UN);

  if (retsize>=0)
   retsize = total_write_bytes;
  printf("Debug mywrite end path %s, size %ld, offset %ld, total write %d\n",path,size,offset,retsize); 
  if (fi->fh == 0)
   fclose(metaptr);
  else
   sem_post(&(file_handle_table[fi->fh].meta_sem));
  return retsize;
 }

int mymknod(const char *path, mode_t filemode,dev_t thisdev)
 {
  struct stat inputstat;
  int retcode=0;
  ino_t this_inode,new_inode;
  FILE *fptr;
  char metapath[1024];
  char filename[1024];
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
   return -ENOENT;
  else
   {
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    if (access(metapath,F_OK)!=0)
     return -ENOENT;
    else
     {
      strcpy(filename,&path[tmpptr-path+1]);

      memset(&inputstat,0,sizeof(struct stat));
      inputstat.st_mode = filemode | S_IFREG;
      inputstat.st_nlink = 1;
      inputstat.st_uid=getuid();
      inputstat.st_gid=getgid();
      inputstat.st_atime=currenttime.time;
      inputstat.st_mtime=currenttime.time;
      inputstat.st_ctime=currenttime.time;

      sem_wait(&mysystem_meta_sem);
      super_inode_create(&inputstat,&new_inode);
      sem_post(&mysystem_meta_sem);

      sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,new_inode % SYS_DIR_WIDTH,new_inode);

      dir_add_filename(this_inode,new_inode,filename);

      fptr=fopen(metapath,"w");
      flock(fileno(fptr),LOCK_EX);
      fwrite(&inputstat,sizeof(struct stat),1,fptr);
      num_blocks = 0;
      fwrite(&num_blocks,sizeof(long),1,fptr);
      flock(fileno(fptr),LOCK_UN);
      fclose(fptr);

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
  char dirname[1024];
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

    if (access(metapath,F_OK)!=0)
     retcode = -ENOENT;
    else
     {

      strcpy(dirname,&path[tmpptr-path+1]);

      /*Done with updating the parent inode meta*/


      memset(&inputstat,0,sizeof(struct stat));
      inputstat.st_mode = S_IFDIR | thismode;
      inputstat.st_nlink = 2;
      inputstat.st_uid=getuid();
      inputstat.st_gid=getgid();
      inputstat.st_atime=currenttime.time;
      inputstat.st_mtime=currenttime.time;
      inputstat.st_ctime=currenttime.time;

      sem_wait(&mysystem_meta_sem);
      super_inode_create(&inputstat,&new_inode);
      sem_post(&mysystem_meta_sem);

      dir_add_dirname(this_inode, new_inode, dirname);

      sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,new_inode % SYS_DIR_WIDTH,new_inode);

      fptr=fopen(metapath,"w");
      flock(fileno(fptr),LOCK_EX);
      fwrite(&inputstat,sizeof(struct stat),1,fptr);
      num_subdir=2;
      num_reg=0;
      fwrite(&num_subdir,sizeof(long),1,fptr);
      fwrite(&num_reg,sizeof(long),1,fptr);
      memset(&ent1,0,sizeof(simple_dirent));
      memset(&ent2,0,sizeof(simple_dirent));
      ent1.st_ino=new_inode;
      ent1.st_mode=S_IFDIR | thismode;
      strcpy(ent1.name,".");
      ent2.st_ino=this_inode;
      ent2.st_mode=S_IFDIR | 0755;
      strcpy(ent2.name,"..");
      fwrite(&ent1,sizeof(simple_dirent),1,fptr);
      fwrite(&ent2,sizeof(simple_dirent),1,fptr);
      flock(fileno(fptr),LOCK_UN);
      fclose(fptr);

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
    flock(fileno(fptr),LOCK_EX);  
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
    flock(fileno(fptr),LOCK_UN);
    fclose(fptr);
    super_inode_write(&inputstat,this_inode);

   }

  return retcode;
 } 

int mychmod(const char *path, mode_t new_mode)
 {

  struct stat inputstat;
  int retcode=0;
  ino_t this_inode;
  FILE *fptr;
  char metapath[1024];
  struct timeb currenttime;

  ftime(&currenttime);

  show_current_time();

  printf("Debug mychmod\n");

  this_inode = find_inode(path);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    fptr = fopen(metapath,"r+");
    if (fptr==NULL)
     return -ENOENT;
    flock(fileno(fptr),LOCK_EX);
    fread(&inputstat,sizeof(struct stat),1,fptr);
    inputstat.st_mode = new_mode;
    fseek(fptr,0,SEEK_SET);
    fwrite(&inputstat,sizeof(struct stat),1,fptr);
    flock(fileno(fptr),LOCK_UN);
    fclose(fptr);
    super_inode_write(&inputstat,this_inode);

   }

  return retcode;
 }

int mychown(const char *path, uid_t new_uid, gid_t new_gid)
 {

  struct stat inputstat;
  int retcode=0;
  ino_t this_inode;
  FILE *fptr;
  char metapath[1024];
  struct timeb currenttime;

  ftime(&currenttime);

  show_current_time();

  printf("Debug mychown\n");

  this_inode = find_inode(path);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    fptr = fopen(metapath,"r+");
    if (fptr==NULL)
     return -ENOENT;
    flock(fileno(fptr),LOCK_EX);
    fread(&inputstat,sizeof(struct stat),1,fptr);
    inputstat.st_uid = new_uid;
    inputstat.st_gid = new_gid;
    fseek(fptr,0,SEEK_SET);
    fwrite(&inputstat,sizeof(struct stat),1,fptr);
    flock(fileno(fptr),LOCK_UN);
    fclose(fptr);
    super_inode_write(&inputstat,this_inode);

   }

  return retcode;
 }

int myunlink(const char *path)
 {
  /*If unlink in FUSE is called before close, FUSE will first
    rename the file to a temp name, then unlink it after the file is closed.*/
  struct stat inputstat;
  int retcode=0;
  ino_t parent_inode,this_inode;
  char *tmpptr;
  int tmpstatus;
  char filename[1024];

  show_current_time();

  printf("Debug myunlink: deleting %s\n",path);
  tmpptr = strrchr(path,'/');

  parent_inode = find_parent_inode(path);
  retcode = mygetattr(path,&inputstat);

  if (retcode < 0)
   return retcode;
  else
   {
    invalidate_inode_cache(path);

    tmpstatus = decrease_nlink_ref(&inputstat);

    if (tmpstatus!=0)
     return -1;

    strcpy(filename,&path[(tmpptr-path)+1]);
    retcode = dir_remove_filename(parent_inode, filename);

   }

  mysync_system_meta();
  printf("Debug unlink: finished unlink\n");


  return retcode;
 }
int myrmdir(const char *path)
 {
  struct stat inputstat;
  int retcode=0;
  ino_t parent_inode,this_inode;
  FILE *fptr;
  char metapath[1024];
  char dirname[1024];
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
    super_inode_reclaim();
    if (tmpstatus!=0)
     return -1;
    sem_wait(&mysystem_meta_sem);
    mysystem_meta.total_inodes -=1;
    sem_post(&mysystem_meta_sem);

    strcpy(dirname,&path[(tmpptr-path)+1]);
    retcode = dir_remove_dirname(parent_inode, dirname);

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
  long old_cache_size, new_cache_size;

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
    flock(fileno(fptr),LOCK_EX);
    fread(&inputstat,sizeof(struct stat),1,fptr);
    fread(&total_blocks,sizeof(long),1,fptr);
    if (length == 0)
     last_block = 0;
    else
     last_block = ((length-1) / (long) MAX_BLOCK_SIZE) + 1;

    printf("Debug truncate: last block is %ld\n",last_block);

    /*First delete blocks that need to be thrown away*/
    /*TODO: Need to be able to delete blocks or truncate blocks on backends as well*/
    for(block_count=last_block+1;block_count<=total_blocks;block_count++)
     {
      printf("Debug truncate: killing block %ld",block_count);
      sprintf(blockpath,"%s/sub_%ld/data_%ld_%ld",BLOCKSTORE,
                                          (this_inode + block_count) % SYS_DIR_WIDTH,this_inode,block_count);
      old_cache_size = check_file_size(blockpath);
      unlink(blockpath);
      sem_wait(&mysystem_meta_sem);
      mysystem_meta.cache_size -= old_cache_size;
      if (mysystem_meta.cache_size < 0)
       mysystem_meta.cache_size = 0;
      sem_post(&mysystem_meta_sem);

     }
    if (length < (last_block * MAX_BLOCK_SIZE))
     {
      sprintf(blockpath,"%s/sub_%ld/data_%ld_%ld",BLOCKSTORE,
                       (this_inode + last_block) % SYS_DIR_WIDTH,this_inode,last_block);

      old_cache_size = check_file_size(blockpath);
      truncate(blockpath, length - ((last_block -1) * MAX_BLOCK_SIZE));
      new_cache_size = check_file_size(blockpath);
      if (old_cache_size!=new_cache_size)
       {
        sem_wait(&mysystem_meta_sem);
        mysystem_meta.cache_size += new_cache_size - old_cache_size;
        if (mysystem_meta.cache_size < 0)
         mysystem_meta.cache_size = 0;
        sem_post(&mysystem_meta_sem);
       }
     }
    total_blocks = last_block;
    sem_wait(&mysystem_meta_sem);
    mysystem_meta.system_size += (length - inputstat.st_size);
    sem_post(&mysystem_meta_sem);
    inputstat.st_size=length;
    inputstat.st_blocks = (inputstat.st_size+511)/512;
    fseek(fptr,0,SEEK_SET);
    fwrite(&inputstat,sizeof(struct stat),1,fptr);
    fwrite(&total_blocks,sizeof(long),1,fptr);
    ftruncate(fileno(fptr),sizeof(struct stat)+sizeof(long)+sizeof(blockent)*last_block);
    flock(fileno(fptr),LOCK_UN);
    fclose(fptr);
    super_inode_write(&inputstat,this_inode);

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

int mycreate(const char *path, mode_t filemode, struct fuse_file_info *fi)
 {
  struct stat inputstat;
  int retcode=0;
  ino_t this_inode,new_inode;
  FILE *fptr;
  char metapath[1024];
  char filename[1024];
  long num_subdir,num_reg,count;
  simple_dirent tempent;
  char *tmpptr;
  long num_blocks =0;
  unsigned int inodehash;
  struct timeb currenttime;
  long count2;
  uint64_t empty_index;
  uint64_t temp_mask;

  ftime(&currenttime);

  tmpptr = strrchr(path,'/');
  show_current_time();


  this_inode = find_parent_inode(path);

  if (this_inode <=0)
   return -ENOENT;
  else
   {
    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);

    if (access(metapath,F_OK)!=0)
     return -ENOENT;
    else
     {
      strcpy(filename,&path[tmpptr-path+1]);

      memset(&inputstat,0,sizeof(struct stat));
      inputstat.st_mode = filemode | S_IFREG;
      inputstat.st_nlink = 1;
      inputstat.st_uid=getuid();
      inputstat.st_gid=getgid();
      inputstat.st_atime=currenttime.time;
      inputstat.st_mtime=currenttime.time;
      inputstat.st_ctime=currenttime.time;

      sem_wait(&mysystem_meta_sem);
      super_inode_create(&inputstat,&new_inode);
      sem_post(&mysystem_meta_sem);

      dir_add_filename(this_inode,new_inode,filename);

      sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,new_inode % SYS_DIR_WIDTH,new_inode);

      printf("debug create using new inode number %ld\n",new_inode);

      fptr=fopen(metapath,"w+");
      flock(fileno(fptr),LOCK_EX);
      setbuf(fptr,NULL);
      fwrite(&inputstat,sizeof(struct stat),1,fptr);
      num_blocks = 0;
      fwrite(&num_blocks,sizeof(long),1,fptr);
      flock(fileno(fptr),LOCK_UN);
      fflush(fptr);

     }
   }

  if (strlen(path)<MAX_ICACHE_PATHLEN)
   {
    inodehash = compute_inode_hash(path);
    replace_inode_cache(inodehash,path,new_inode);
   }
  mysync_system_meta();
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
  file_handle_table[empty_index].st_ino = new_inode;
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

  file_handle_table[empty_index].metaptr=fptr;
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

  sem_init(&(file_handle_table[empty_index].meta_sem),0,1);
  sem_init(&(file_handle_table[empty_index].block_sem),0,1);

  memcpy(&(file_handle_table[empty_index].inputstat),&inputstat, sizeof(struct stat));
  file_handle_table[empty_index].total_blocks = num_blocks;

  sem_post(&file_table_sem);
  printf("Debug end of mycreate\n");
  show_current_time();

  return 0;
 }

int myrename(const char *oldpath, const char *newpath)
 {
  struct stat oldpathstat;
  struct stat newpathstat;
  FILE *fptr;
  int retcode,newretcode;
  char metapath[1024];
  char blockpath[1024];
  char oldname[512];
  char newname[512];
  long sizebuf[2];
  ino_t oldparent_ino, newparent_ino;
  char *old_tmpptr,*new_tmpptr;
  long count,old_entryindex,new_entryindex;
  simple_dirent tempent, old_tempent, new_tempent;
  int tmpstatus;
  long block_count, total_blocks;

  printf("Debug renaming from %s to %s\n",oldpath,newpath);

  if ((strcmp(oldpath,"/")==0) || (strcmp(newpath,"/")==0))
   return -EACCES;

  retcode=mygetattr(oldpath,&oldpathstat);
  if (retcode < 0)
   return retcode;

  if (oldpathstat.st_mode & S_IFDIR)
   {
    newretcode = mygetattr(newpath, &newpathstat);
    if ((newretcode !=-ENOENT) && (newretcode < 0))
     return newretcode;

    if ((newretcode >= 0) && ( newpathstat.st_mode & S_IFDIR)) /*Check if the target directory is empty*/
     {
      sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,newpathstat.st_ino % SYS_DIR_WIDTH,newpathstat.st_ino);
      fptr = fopen(metapath,"r");
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fread(sizebuf,sizeof(long),2,fptr);
      fclose(fptr);
      if ((sizebuf[0]+sizebuf[1]) > 2) /*Non-empty directory*/
       return -ENOTEMPTY;
      myrmdir(newpath);
     }
    /*Can proceed to rename oldpath to newpath*/
   }
  else
   {
    newretcode = mygetattr(newpath, &newpathstat);
    if ((newretcode !=-ENOENT) && (retcode < 0))
     return newretcode;
   }
  if (newpathstat.st_ino == oldpathstat.st_ino)
   return 0;

  oldparent_ino = find_parent_inode(oldpath);
  newparent_ino = find_parent_inode(newpath);

  invalidate_inode_cache(oldpath);
  if (newretcode >=0)
   invalidate_inode_cache(newpath);

  if (oldparent_ino == newparent_ino) /*Within the same dir*/
   {
    if (newretcode == -ENOENT) /*Just rename*/
     {
      old_tmpptr = strrchr(oldpath,'/');
      new_tmpptr = strrchr(newpath,'/');

      strcpy(oldname,&oldpath[old_tmpptr-oldpath+1]);
      strcpy(newname,&newpath[new_tmpptr-newpath+1]);

      sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,oldparent_ino % SYS_DIR_WIDTH,oldparent_ino);
      fptr = fopen(metapath,"r+");
      flock(fileno(fptr),LOCK_EX);
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fread(sizebuf,sizeof(long),2,fptr);

      for(count = 0; count<(sizebuf[0]+sizebuf[1]);count++)
       {
        fread(&tempent,sizeof(simple_dirent),1,fptr);
        if (tempent.st_ino == oldpathstat.st_ino)
         {
          strcpy(tempent.name,newname);
          fseek(fptr,sizeof(struct stat)+2*sizeof(long)+sizeof(simple_dirent)*count,SEEK_SET);
          fwrite(&tempent,sizeof(simple_dirent),1,fptr);
          flock(fileno(fptr),LOCK_UN);
          fclose(fptr);
          return 0;
         }
       }
      /*Cannot find old entry?*/
      flock(fileno(fptr),LOCK_UN);
      fclose(fptr);
      return -ENOENT;
     }
    /* Target path should be a file or sym link*/

    if (newpathstat.st_mode & S_IFDIR)
     return -EACCES;

    old_tmpptr = strrchr(oldpath,'/');
    new_tmpptr = strrchr(newpath,'/');

    strcpy(oldname,&oldpath[old_tmpptr-oldpath+1]);
    strcpy(newname,&newpath[new_tmpptr-newpath+1]);

    sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,oldparent_ino % SYS_DIR_WIDTH,oldparent_ino);
    fptr = fopen(metapath,"r+");
    flock(fileno(fptr),LOCK_EX);
    fseek(fptr,sizeof(struct stat),SEEK_SET);
    fread(sizebuf,sizeof(long),2,fptr);

    old_entryindex = 0;
    new_entryindex = 0;

    for(count = 0; count<(sizebuf[0]+sizebuf[1]);count++)
     {
      fread(&tempent,sizeof(simple_dirent),1,fptr);
      if (tempent.st_ino == oldpathstat.st_ino)
       {
        memcpy(&old_tempent,&tempent,sizeof(simple_dirent));
        old_entryindex=count;
       }
      if (tempent.st_ino == newpathstat.st_ino)
       {
        memcpy(&new_tempent,&tempent,sizeof(simple_dirent));
        new_entryindex=count;
       }
      if ((old_entryindex !=0) && (new_entryindex !=0))
       break;
     }
    strcpy(old_tempent.name,newname);
    memset(&new_tempent,0,sizeof(simple_dirent));
    fseek(fptr,sizeof(struct stat)+2*sizeof(long)+sizeof(simple_dirent)*old_entryindex,SEEK_SET);
    fwrite(&old_tempent,sizeof(simple_dirent),1,fptr);
    fseek(fptr,sizeof(struct stat)+2*sizeof(long)+sizeof(simple_dirent)*new_entryindex,SEEK_SET);
    fwrite(&new_tempent,sizeof(simple_dirent),1,fptr);
    fflush(fptr);

    sizebuf[1]--;
    fseek(fptr,sizeof(struct stat)+sizeof(long),SEEK_SET);
    fwrite(&(sizebuf[1]),sizeof(long),1,fptr);
    if (new_entryindex<(sizebuf[1]+sizebuf[0]))  /*If the entry to be deleted is not at the end of the meta*/
     {
      fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+((sizebuf[1]+sizebuf[0])*sizeof(simple_dirent)),SEEK_SET);
      fread(&tempent,sizeof(simple_dirent),1,fptr);
      fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+(new_entryindex*sizeof(simple_dirent)),SEEK_SET);
      fwrite(&tempent,sizeof(simple_dirent),1,fptr);
     }
    ftruncate(fileno(fptr),sizeof(struct stat)+(2*sizeof(long))+((sizebuf[1]+sizebuf[0])*sizeof(simple_dirent)));
    flock(fileno(fptr),LOCK_UN);
    fclose(fptr);

    tmpstatus = decrease_nlink_ref(&newpathstat);
    if (tmpstatus !=0)
     return tmpstatus;

    mysync_system_meta();
    return 0;
   }
  else    /* If not in the same directory */
   {
    if (newretcode == -ENOENT) /* Just move to the new parent directory */
     {
      old_tmpptr = strrchr(oldpath,'/');
      new_tmpptr = strrchr(newpath,'/');

      strcpy(oldname,&oldpath[old_tmpptr-oldpath+1]);
      strcpy(newname,&newpath[new_tmpptr-newpath+1]);

      if (oldpathstat.st_mode & S_IFDIR)
       {
        dir_add_dirname(newparent_ino, oldpathstat.st_ino, newname);
        dir_remove_dirname(oldparent_ino, oldname);
       }
      else
       {
        dir_add_filename(newparent_ino, oldpathstat.st_ino, newname);
        dir_remove_filename(oldparent_ino, oldname);
       }

      return 0;
     }
    /* Target path should be a file or sym link*/

    if (newpathstat.st_mode & S_IFDIR)
     return -EACCES;

    old_tmpptr = strrchr(oldpath,'/');
    new_tmpptr = strrchr(newpath,'/');

    strcpy(oldname,&oldpath[old_tmpptr-oldpath+1]);
    strcpy(newname,&newpath[new_tmpptr-newpath+1]);

    if (oldpathstat.st_mode & S_IFDIR)
     {
      dir_remove_filename(newparent_ino, newname);
      dir_add_dirname(newparent_ino, oldpathstat.st_ino, newname);
      dir_remove_dirname(oldparent_ino, oldname);
     }
    else
     {
      dir_remove_filename(newparent_ino, newname);
      dir_add_filename(newparent_ino, oldpathstat.st_ino, newname);
      dir_remove_filename(oldparent_ino, oldname);
     }

    tmpstatus = decrease_nlink_ref(&newpathstat);
    if (tmpstatus !=0)
     return tmpstatus;

    mysync_system_meta();
    return 0;
   }

  return 0;
 }


