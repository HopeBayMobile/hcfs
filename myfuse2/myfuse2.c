/* Code under development by Jiahong Wu*/

#include "myfuse.h"

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
 };

void main(int argc, char **argv)
 {

  if (argc < 2)
   {
    printf("Not enough arguments\n");
    return;
   }

  initsystem();
  fuse_main(argc,argv,&my_fuse_ops,NULL);


  return;
 }
