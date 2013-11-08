/* Code under development by Jiahong Wu*/

#include "myfuse.h"
#include "mycurl.h"

void show_current_time()
 {
  struct timeb currenttime;
  char printedtime[100];
  
  ftime(&currenttime);  
  printf("%s.%d\n", ctime_r(&(currenttime.time),printedtime),currenttime.millitm);
  return;
 }

static struct fuse_operations my_fuse_ops = {
  .getattr = mygetattr,
  .readdir = myreaddir,
  .open = myopen,
  .opendir = myopendir,
  .read = myread,
  .write = mywrite,
  .mknod = mymknod,
  .utime = myutime,
  .rename = myrename,
  .unlink = myunlink,
  .fsync = myfsync,
  .mkdir = mymkdir,
  .rmdir = myrmdir,
  .destroy = mydestroy,
  .truncate = mytruncate,
  .release = myrelease,
  .statfs = mystatfs,
  .create = mycreate,
  .chmod = mychmod,
  .chown = mychown,
 };

void main(int argc, char **argv)
 {
  pid_t this_pid,this_pid1;
  int upload_conn_count;
  FILE *fptr;

  if (argc < 2)
   {
    printf("Not enough arguments\n");
    return;
   }

  initsystem();
  pipe(delete_pipe);
  this_pid = fork();
  if (this_pid ==0)
   {
    fptr=fopen("fuse_log","a+");
    fprintf(fptr,"\nStart logging fuse\n");
    printf("Redirecting to fuse log\n");
    dup2(fileno(fptr),fileno(stdout));
    dup2(fileno(fptr),fileno(stderr));
    close(delete_pipe[0]);   /* FUSE process only write to-deletes to the pipe */
    sem_init(&download_curl_sem,0,MAX_CURL_HANDLE);
    sem_init(&delete_enqueue_sem,0,1);
    for(upload_conn_count=0;upload_conn_count<MAX_CURL_HANDLE;upload_conn_count++)
     {
      curl_handle_mask[upload_conn_count]=False;
      if (init_swift_backend(&(download_curl_handles[upload_conn_count]))!=0)
       {
        printf("error in connecting to swift\n");
        exit(0);
       }
     }
    fuse_main(argc,argv,&my_fuse_ops,NULL);
    for(upload_conn_count=0;upload_conn_count<MAX_CURL_HANDLE;upload_conn_count++)
     {
      destroy_swift_backend(download_curl_handles[upload_conn_count].curl);
     }
    fclose(fptr);
   }
  else
   {
    this_pid1 = fork();
    if (this_pid1 == 0)
     {
      fptr=fopen("swift_upload_log","a+");
      fprintf(fptr,"\nStart logging swift upload\n");
      printf("Redirecting to swift log\n");
      dup2(fileno(fptr),fileno(stdout));
      dup2(fileno(fptr),fileno(stderr));
      close(delete_pipe[1]);  /* datasync process only read from the pipe */
      sem_init(&delete_queue_sem,0,1);
      run_maintenance_loop();
      fclose(fptr);
     }
    else
     {
      fptr=fopen("cache_maintain_log","a+");
      fprintf(fptr,"\nStart logging cache cleanup\n");
      printf("Redirecting to cache log\n");
      dup2(fileno(fptr),fileno(stdout));
      dup2(fileno(fptr),fileno(stderr));
      close(delete_pipe[0]);
      close(delete_pipe[1]);   /* cache replacement process does not interact with the pipe */

      run_cache_loop();
      fclose(fptr);
     }
   }

  printf("End of main process\n");

  return;
 }
