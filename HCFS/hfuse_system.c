#include "fuseop.h"
#include "super_inode.h"
#include "dir_lookup.h"
#include "hcfscurl.h"
#include "hcfs_tocloud.h"
#include "params.h"
#include <fuse.h>
#include <sys/ipc.h>
#include <sys/shm.h>


int init_hcfs_system_data()
 {
  int shm_key;

  shm_key = shmget(2345,sizeof(SYSTEM_DATA_HEAD), IPC_CREAT| 0666);
  hcfs_system = shmat(shm_key, NULL, 0);

  memset(hcfs_system,0,sizeof(SYSTEM_DATA_HEAD));
  sem_init(&(hcfs_system->access_sem),1,1);
  sem_init(&(hcfs_system->num_cache_sleep_sem),1,0);
  sem_init(&(hcfs_system->check_cache_sem),1,0);
  sem_init(&(hcfs_system->check_next_sem),1,0);
  
  hcfs_system->system_val_fptr = fopen(HCFSSYSTEM,"r+");
  if (hcfs_system->system_val_fptr == NULL)
   {
    hcfs_system->system_val_fptr = fopen(HCFSSYSTEM,"w+");
    fwrite(&(hcfs_system->systemdata),sizeof(SYSTEM_DATA_TYPE),1,hcfs_system->system_val_fptr);
    fclose(hcfs_system->system_val_fptr);
    hcfs_system->system_val_fptr = fopen(HCFSSYSTEM,"r+");
   }
  setbuf(hcfs_system->system_val_fptr,NULL);
  fread(&(hcfs_system->systemdata),sizeof(SYSTEM_DATA_TYPE),1,hcfs_system->system_val_fptr);

  return 0;
 }

int sync_hcfs_system_data(char need_lock)
 {
  if (need_lock == TRUE)
   sem_wait(&(hcfs_system->access_sem));
  fseek(hcfs_system->system_val_fptr,0,SEEK_SET);
  fwrite(&(hcfs_system->systemdata),sizeof(SYSTEM_DATA_TYPE),1,hcfs_system->system_val_fptr);
  if (need_lock == TRUE)
   sem_post(&(hcfs_system->access_sem));

  return 0;
 }

void init_hfuse()
 {
  int ret_val;
  char rootmetapath[400];
  ino_t root_inode;
  struct stat this_stat;
  DIR_META_TYPE this_meta;
  DIR_ENTRY_PAGE temppage;
  mode_t self_mode;
  FILE *metafptr;

  ret_val = super_inode_init();
  init_pathname_cache();
  ret_val = init_system_fh_table();
  init_hcfs_system_data();

  /* Check if need to initialize the root meta file */
  fetch_meta_path(rootmetapath,1);
  if (access(rootmetapath,F_OK)!=0)
   {
    memset(&this_stat,0,sizeof(struct stat));
    memset(&this_meta,0,sizeof(DIR_META_TYPE));
    memset(&temppage,0,sizeof(DIR_ENTRY_PAGE));

    self_mode = S_IFDIR | 0755;
    this_stat.st_mode = self_mode;
    this_stat.st_nlink = 2;   /*One pointed by the parent, another by self*/
    this_stat.st_uid = getuid();
    this_stat.st_gid = getgid();
    this_stat.st_atime = time(NULL);
    this_stat.st_mtime = this_stat.st_atime;
    this_stat.st_ctime = this_stat.st_ctime;

    root_inode = super_inode_new_inode(&this_stat);
    /*TODO: put error handling here if root_inode is not 1 (cannot initialize system)*/

    this_stat.st_ino = 1;
    memcpy(&(this_meta.thisstat),&this_stat,sizeof(struct stat));

    metafptr = fopen(rootmetapath,"w");

    ret_val = fwrite(&this_meta,sizeof(DIR_META_TYPE),1,metafptr);

    this_meta.next_subdir_page = ftell(metafptr);
    fseek(metafptr,0,SEEK_SET);

    ret_val = fwrite(&this_meta,sizeof(DIR_META_TYPE),1,metafptr);

    temppage.num_entries = 2;
    temppage.dir_entries[0].d_ino = 1;
    temppage.dir_entries[1].d_ino = 0;
    strcpy(temppage.dir_entries[0].d_name,".");
    strcpy(temppage.dir_entries[1].d_name,"..");

    ret_val = fwrite(&temppage,sizeof(DIR_ENTRY_PAGE),1,metafptr);
    fclose(metafptr);
   }
  return;
 }

int main(int argc, char **argv)
 {
  CURL_HANDLE curl_handle;
  int ret_val;
  pid_t this_pid, this_pid1;
  int download_handle_count;
  
  sprintf(curl_handle.id,"main");
  ret_val = hcfs_init_swift_backend(&curl_handle);
  if ((ret_val < 200) || (ret_val > 299))
   {
    printf("error in connecting to swift\n");
    exit(0);
   }

  printf("%s\n %s\n %d\n",swift_auth_string,swift_url_string,ret_val);
  ret_val = hcfs_swift_list_container(&curl_handle);
  if ((ret_val < 200) || (ret_val > 299))
   {
    printf("error in connecting to swift\n");
    exit(0);
   }
  printf("ret code %d\n",ret_val);
  hcfs_destroy_swift_backend(curl_handle.curl);
  init_hfuse();
  this_pid = fork();
  if (this_pid == 0)
   {
    this_pid1 = fork();
    if (this_pid1 == 0)
     {
      logfptr=fopen("swift_upload_log","a+");
      fprintf(logfptr,"\nStart logging swift upload\n");
      printf("Redirecting to swift log\n");
      dup2(fileno(logfptr),fileno(stdout));
      dup2(fileno(logfptr),fileno(stderr));

      upload_loop();
      fclose(logfptr);
     }
    else
     {
      logfptr=fopen("cache_maintain_log","a+");
      fprintf(logfptr,"\nStart logging cache cleanup\n");
      printf("Redirecting to cache log\n");
      dup2(fileno(logfptr),fileno(stdout));
      dup2(fileno(logfptr),fileno(stderr));

      run_cache_loop();
      fclose(logfptr);
     }
   }
  else
   {
    logfptr=fopen("fuse_log","a+");
    fprintf(logfptr,"\nStart logging fuse\n");
    printf("Redirecting to fuse log\n");
    dup2(fileno(logfptr),fileno(stdout));
    dup2(fileno(logfptr),fileno(stderr));
//    close(delete_pipe[0]);   /* FUSE process only write to-deletes to the pipe */
    sem_init(&download_curl_sem,0,MAX_DOWNLOAD_CURL_HANDLE);
//    sem_init(&delete_enqueue_sem,0,1);

    for(download_handle_count=0;download_handle_count<MAX_DOWNLOAD_CURL_HANDLE;download_handle_count++)
     {
      curl_handle_mask[download_handle_count]=FALSE;
      ret_val = hcfs_init_swift_backend(&(download_curl_handles[download_handle_count]));
      while ((ret_val < 200) || (ret_val > 299))
       {
        printf("error in connecting to swift\n");
        if (download_curl_handles[download_handle_count].curl !=NULL)
         hcfs_destroy_swift_backend(download_curl_handles[download_handle_count].curl);
        ret_val = hcfs_init_swift_backend(&(download_curl_handles[download_handle_count]));

       }
     }
    hook_fuse(argc,argv);
    return;
   }
  return;
 }
