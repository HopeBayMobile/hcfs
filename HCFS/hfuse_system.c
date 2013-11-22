#include "fuseop.h"
#include "super_inode.h"
#include "dir_lookup.h"
#include "hcfscurl.h"
#include "hcfs_tocloud.h"
#include <fuse.h>

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
//  init_hcfs_system_data();

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
  
  sprintf(curl_handle.id,"main");
  ret_val = hcfs_init_swift_backend(&curl_handle);
  printf("%s\n %s\n %d\n",swift_auth_string,swift_url_string,ret_val);
  ret_val = hcfs_swift_list_container(&curl_handle);
  printf("ret code %d\n",ret_val);
  hcfs_destroy_swift_backend(curl_handle.curl);
  init_hfuse();
  this_pid = fork();
  if (this_pid == 0)
   {
    upload_loop();
   }
  else
   {
    return hook_fuse(argc,argv);
   }
  return;
 }
